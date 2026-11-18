package dev.silphmod.nativecheck;

import android.content.*;
import android.os.SystemClock;
import dev.rootcatch.nativeengine.*;
import dev.rootcatch.root.RootShell;
import dev.rootcatch.ugphone.KecleonMovement;
import java.util.*;
import java.util.concurrent.*;
import java.util.function.BooleanSupplier;

final class NativeHuntingHost implements AutoCloseable {
    private final NativeSessionController controller;private final CheckLog log;private final BooleanSupplier active;
    private final SharedPreferences settings,history,raidHistory;private final RootShell joystick=new RootShell();
    private final ScheduledExecutorService network=Executors.newSingleThreadScheduledExecutor(r->new Thread(r,"silph-hunting-feed"));
    private final ExecutorService worker=Executors.newSingleThreadExecutor(r->new Thread(r,"silph-hunting-route"));
    private volatile HuntingFeed.Result feed;private volatile NativeKecleonWire.Event event;private volatile NativeSessionWire.Snapshot latest;
    private volatile HuntingFeed.Candidate candidate;private volatile boolean closed;private volatile String account="",waiting="Waiting for candidates…";
    private final LinkedHashSet<String> checked=new LinkedHashSet<>(),ids=new LinkedHashSet<>();private final Map<String,Long> retry=new HashMap<>();private final Map<String,Integer> attempts=new HashMap<>();
    private final HuntingBackoff areas=new HuntingBackoff();
    private long nextFeed,commandSequence,choosingRequest,authorization,acknowledged,throwsSeen,berriesSeen,nextTick,selectedAt;private volatile long lastAction;
    private volatile double lastLatitude,lastLongitude;private double anchorLatitude,anchorLongitude,latitude,longitude;private double mapAverage=12000;
    private int feedFailures;private boolean choosing,latencyRecorded;private long waitLogged,dataMs;
    NativeHuntingHost(Context context,NativeSessionController controller,CheckLog log,BooleanSupplier active){
        this.controller=controller;this.log=log;this.active=active;settings=new NativeSettings(context).prefs;history=context.getSharedPreferences("hunting-history",0);raidHistory=context.getSharedPreferences("raid-history",0);RaidTime.init(context);
        network.scheduleWithFixedDelay(this::refresh,0,15,TimeUnit.SECONDS);
    }
    String waiting(){return waiting;}
    private void refresh(){
        long now=SystemClock.elapsedRealtime();if(closed||now<nextFeed)return;nextFeed=now+60000;
        try{HuntingFeed.Result result=HuntingFeed.fetch();if(closed)return;RaidTime.observe(result.utc+(SystemClock.elapsedRealtime()-result.received),SystemClock.elapsedRealtime());feed=result;feedFailures=0;submit(()->{Set<String> live=new HashSet<>();for(HuntingFeed.Candidate row:result.candidates)live.add(row.key);retry.keySet().retainAll(live);attempts.keySet().retainAll(live);});log.write("hunting_feed","HTTP=200 rows="+result.rows+" usable="+result.candidates.size()+" invalid="+result.invalid+" expired="+result.expired+" shinyUnavailable="+result.shinyUnavailable+" serverMs="+result.utc+" requestMs="+result.requestMs);tick();}
        catch(Exception error){feedFailures++;nextFeed=now+Math.min(120000,15000L*(1L<<Math.min(3,feedFailures-1)));if(!closed){waiting="Candidate source unavailable";log.write("hunting_feed_error",error.toString());}}
    }
    void snapshot(NativeSessionWire.Snapshot value){
        NativeSessionWire.Snapshot previous=latest;latest=value;if(closed)return;
        if(previous==null||value.throwsCount>previous.throwsCount||value.berries>previous.berries||value.caught>previous.caught)submit(()->recordSnapshot(value));
    }
    private void recordSnapshot(NativeSessionWire.Snapshot value){
        if(closed||account.isEmpty())return;
        if(value.throwsCount>throwsSeen||value.berries>berriesSeen){throwsSeen=value.throwsCount;berriesSeen=value.berries;HuntingFeed.Candidate current=candidate;if(current!=null&&value.candidateRequest==choosingRequest){long utc=RaidTime.now();if(utc>0){recordAction(utc,current.latitude,current.longitude);log.write("hunting_cooldown","request="+choosingRequest+" at="+utc+" latitude="+lastLatitude+" longitude="+lastLongitude+" throws="+throwsSeen+" berries="+berriesSeen);}}}
        int max=settings.getInt("hunting.maxCatches",0);if(max>0&&value.caught>=max)controller.requestStop("Hunting catch limit reached");
    }
    synchronized void accept(NativeKecleonWire.Event value){
        if(closed)return;String key=Long.toUnsignedString(value.accountHi,16)+":"+Long.toUnsignedString(value.accountLo,16);
        if(!account.isEmpty()&&!account.equals(key)){controller.requestStop("Hunting account changed");return;}
        if(account.isEmpty()){
            account=key;checked.addAll(history.getStringSet("checked."+account,Collections.emptySet()));ids.addAll(history.getStringSet("ids."+account,Collections.emptySet()));anchorLatitude=latitude=value.latitude;anchorLongitude=longitude=value.longitude;
            loadAction(history);loadAction(raidHistory);log.write("hunting_session","accountBound=true checked="+checked.size()+" knownIds="+ids.size()+" reference="+latitude+","+longitude+" radiusKm="+settings.getInt("hunting.radiusKm",0));
        }
        event=value;submit(()->process(value));
    }
    private void loadAction(SharedPreferences source){long at=source.getLong("time."+account,0);if(at<=lastAction)return;try{double[] point=RaidActionHistory.coordinates(source.getString("location."+account,""));lastAction=at;lastLatitude=point[0];lastLongitude=point[1];}catch(IllegalArgumentException ignored){}}
    private void recordAction(long at,double lat,double lon){lastAction=at;lastLatitude=lat;lastLongitude=lon;RaidActionHistory.record(history,account,at,lat,lon);RaidActionHistory.record(raidHistory,account,at,lat,lon);}
    private interface Job{void run()throws Exception;}
    private void submit(Job job){if(closed)return;try{worker.execute(()->{try{if(!closed)job.run();}catch(InterruptedException e){Thread.currentThread().interrupt();}catch(Exception e){if(!closed){log.write("hunting_host_error",e.toString());controller.requestStop("Hunting controller: "+e.getMessage());}}});}catch(RejectedExecutionException ignored){}}
    synchronized void tick(){long now=SystemClock.elapsedRealtime();NativeKecleonWire.Event e=event;if(closed||e==null||choosing||now<nextTick||!(e.phase==1||e.phase==4))return;choosing=true;nextTick=now+1000;submit(()->{try{process(e);}finally{synchronized(this){choosing=false;}}});}
    private void send(NativeKecleonWire.Event e,int kind,boolean accepted,double lat,double lon,String source){if(closed||event==null||event.request!=e.request)return;controller.enqueue(NativeKecleonWire.command(e,++commandSequence,kind,0,accepted,lat,lon,source));}
    private void waitFor(String text){waiting=text;long now=SystemClock.elapsedRealtime();if(now-waitLogged>=30000){waitLogged=now;log.write("hunting_wait",text);}}
    private void process(NativeKecleonWire.Event e)throws Exception{
        if(closed||event==null||e.sequence!=event.sequence||!active.getAsBoolean())return;
        if(e.phase==1){
            if(candidate!=null)return;latitude=e.latitude;longitude=e.longitude;HuntingFeed.Result source=feed;long utc=RaidTime.now(),elapsed=SystemClock.elapsedRealtime();if(source==null||utc<0){waitFor("Waiting for candidate source…");return;}
            if(elapsed-source.received>180000){waitFor("Refreshing candidate source…");return;}
            HuntingFeed.Candidate best=null;double score=Double.POSITIVE_INFINITY;long minimumLife=(long)Math.max(8000,Math.min(25000,mapAverage+3000));int radius=settings.getInt("hunting.radiusKm",0);boolean unavailable=false;
            for(HuntingFeed.Candidate row:source.candidates){if(row.expires<=utc+minimumLife||checked.contains(row.key)||retry.getOrDefault(row.key,0L)>elapsed)continue;if(radius>0&&RaidFeed.distanceMeters(anchorLatitude,anchorLongitude,row.latitude,row.longitude)>radius*1000.0)continue;
                if(areas.blocked(row.latitude,row.longitude,elapsed)){unavailable=true;continue;}
                long remaining=lastAction>0?RaidCooldown.remainingMillis(RaidFeed.distanceMeters(lastLatitude,lastLongitude,row.latitude,row.longitude),utc-lastAction):0;
                double distance=RaidFeed.distanceMeters(latitude,longitude,row.latitude,row.longitude);double candidateScore=distance+attempts.getOrDefault(row.key,0)*2500.0+(remaining>=row.expires-utc-2000?20000000:remaining>0?10000000:0);if(candidateScore<score){score=candidateScore;best=row;}}
            if(best==null){waitFor(unavailable?"Waiting for candidate data…":"Waiting for new 100 IV candidates…");return;}
            candidate=best;choosingRequest=e.request;authorization=0;acknowledged=0;selectedAt=elapsed;latencyRecorded=false;dataMs=0;waiting="Checking "+best.name;attempts.merge(best.key,1,Integer::sum);
            int budget=budget(attempts.get(best.key));long ttl=Math.min(3600000,best.expires-utc);
            String order=settings.getString("hunting.balls","3,2,1");List<Integer> selected=new ArrayList<>();for(String part:order.split(",")){int id=Integer.parseInt(part);if((id<1||id>3)&&id!=7||selected.contains(id)||selected.size()>=4)throw new IllegalArgumentException("Invalid hunting ball priority");selected.add(id);}if(selected.isEmpty())throw new IllegalArgumentException("No hunting balls selected");while(selected.size()<4)selected.add(0);
            String descriptor=best.species+":"+best.cp+":"+best.level+":"+budget+":"+ttl+":"+selected.get(0)+"."+selected.get(1)+"."+selected.get(2)+"."+selected.get(3);
            send(e,1,false,best.latitude,best.longitude,descriptor);log.write("hunting_candidate","request="+e.request+" species="+best.species+" cp="+best.cp+" level="+best.level+" latitude="+best.latitude+" longitude="+best.longitude+" expires="+best.expires+" budgetMs="+budget+" attempt="+attempts.get(best.key));
        }else if(e.phase==2){
            HuntingFeed.Candidate current=candidate;if(current==null||choosingRequest!=e.request)return;waiting="Moving to candidate…";RootShell.Result result=joystick.execute(KecleonMovement.command(current.latitude,current.longitude),10000);boolean ok=result.code==0&&!result.output.contains("Error")&&!result.output.contains("Exception");if(closed||!active.getAsBoolean())return;send(e,2,ok,0,0,"");log.write("hunting_move","request="+e.request+" accepted="+ok);
        }else if(e.phase==3)waiting="Waiting for candidate data…";
        else if(e.phase==4){
            HuntingFeed.Candidate current=candidate;if(current==null||choosingRequest!=e.request||authorization==e.sequence)return;if(e.verified!=1||e.fort.isEmpty()||e.incident.isEmpty()){send(e,3,false,0,0,"");return;}
            recordLatency(SystemClock.elapsedRealtime());
            if(ids.contains(e.incident)){send(e,3,false,0,0,"");log.write("hunting_duplicate","id="+e.incident);return;}
            long utc=RaidTime.now();if(utc<0){waitFor("Synchronizing capture time…");return;}if(utc>=current.expires-2000){send(e,3,false,0,0,"");return;}
            long remaining=lastAction>0?RaidCooldown.remainingMillis(RaidFeed.distanceMeters(lastLatitude,lastLongitude,current.latitude,current.longitude),utc-lastAction):0;
            if(remaining>0){if(remaining>=current.expires-utc-2000){send(e,3,false,0,0,"");return;}waitFor("Shiny 100 IV · Cooldown "+Math.max(1,(remaining+999)/1000)+" s");return;}
            authorization=e.sequence;recordAction(utc,current.latitude,current.longitude);send(e,3,true,0,0,"");waiting="Capturing shiny 100 IV…";log.write("hunting_capture_authorized","request="+e.request+" encounter="+e.incident+" verifiedShinyHundo=true cooldownRemaining=0 allCatchesKept=true");
        }else if(e.phase==5)waiting="Capturing shiny 100 IV…";
        else if(e.phase==7){
            if(acknowledged==e.request)return;acknowledged=e.request;HuntingFeed.Candidate current=candidate;long elapsed=SystemClock.elapsedRealtime();if(current!=null){
                long spent=elapsed-selectedAt;boolean confirmed=e.reason==1||e.reason==2||e.reason==6||e.reason==7;if(!e.incident.isEmpty()&&confirmed){areas.success(current.latitude,current.longitude);checked.add(current.key);ids.add(e.incident);recordLatency(elapsed);}
                else if(attempts.getOrDefault(current.key,0)>=2||e.reason==2)checked.add(current.key);else retry.put(current.key,elapsed+20000);
                if(!confirmed&&areas.failure(current.latitude,current.longitude,elapsed))log.write("hunting_area_backoff","latitude="+current.latitude+" longitude="+current.longitude+" retryAfterMs=60000");
                trim(checked,4000);trim(ids,4000);history.edit().putStringSet("checked."+account,new LinkedHashSet<>(checked)).putStringSet("ids."+account,new LinkedHashSet<>(ids)).apply();log.write("hunting_result","request="+e.request+" reason="+e.reason+" encounter="+e.incident+" elapsedMs="+spent+" dataMs="+dataMs+" nextBudgetMs="+budget(1));}
            candidate=null;send(e,4,false,0,0,"");waiting="Finding candidates…";
        }else if(e.phase==8)controller.requestStop(e.reason==9?"Hunting account changed":e.reason==5?"Hunting limit reached":"Pokémon storage or Poké Balls unavailable");
    }
    private int budget(int attempt){return (int)Math.min(32000,Math.max(10000,Math.min(24000,mapAverage*1.25+2000))+(attempt>1?8000:0));}
    private void recordLatency(long now){if(latencyRecorded)return;latencyRecorded=true;dataMs=Math.max(0,now-selectedAt);if(dataMs>0&&dataMs<45000)mapAverage=mapAverage*.75+Math.max(1000,dataMs)*.25;}
    private static void trim(Set<String> values,int limit){while(values.size()>limit){Iterator<String> it=values.iterator();it.next();it.remove();}}
    @Override public synchronized void close(){closed=true;network.shutdownNow();worker.shutdownNow();joystick.close();}
}
