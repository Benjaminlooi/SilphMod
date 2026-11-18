#include <android/log.h>
#include <math.h>
typedef struct {
    void *read,*managed,*game,*rpc;
    SilphScheduleIdentity identity;
    SilphGameStop stop;
    uint64_t request,next_query,sent,received,poll_after,negative_at;
    uint32_t move,pending,disabled,failures,displays,types,future,expired,completed,negative_count;
    char negative_fort[161];
} KecleonDiscovery;
static double discovery_distance(double a,double b,double c,double d){double r=.017453292519943295,x=sin((a-c)*r/2),y=sin((b-d)*r/2);return 12742000*asin(sqrt(fmax(0,fmin(1,x*x+cos(a*r)*cos(c*r)*y*y))));}
static int discovery_drop(KecleonDiscovery *s,SilphManagedRef *ref){if(!ref->serial)return 0;int r=silph_managed_release(s->managed,*ref);if(!r)*ref=(SilphManagedRef){0};return r;}
static int discovery_scalar(KecleonDiscovery *s,SilphManagedRef ref,const char *name,uint32_t kind,uint64_t *out){SilphReadValue v={0};int r=silph_read_field(s->read,ref,name,kind,&v);if(!r)*out=v.bits;return r;}
static int discovery_ref(KecleonDiscovery *s,SilphManagedRef ref,const char *name,SilphManagedRef *out){SilphReadValue v={0};int r=silph_read_field(s->read,ref,name,SILPH_READ_REFERENCE,&v);if(!r)*out=v.reference;return r;}
static int discovery_text(KecleonDiscovery *s,SilphManagedRef ref,const char *name,char out[161]){SilphManagedRef text={0};uint32_t n=0;int r=discovery_ref(s,ref,name,&text);if(!r)r=silph_read_utf8(s->read,text,out,161,&n);int d=discovery_drop(s,&text);return r?r:d;}
static int discovery_count(KecleonDiscovery *s,SilphManagedRef ref,unsigned maximum,unsigned *out){
    *out=0;if(!ref.serial)return 0;SilphReadValue v={0};int r=silph_read_call(s->read,ref,"get_Count",SILPH_READ_I32,NULL,0,&v);if(!r&&v.bits>maximum)r=SILPH_MANAGED_CAPACITY;if(!r)*out=(unsigned)v.bits;return r;
}
static int discovery_item(KecleonDiscovery *s,SilphManagedRef list,unsigned index,SilphManagedRef *out){SilphReadValue v={0};SilphReadArgument a={.kind=SILPH_READ_I32,.value.bits=index};int r=silph_read_call(s->read,list,"get_Item",SILPH_READ_REFERENCE,&a,1,&v);if(!r)*out=v.reference;return r;}
static int discovery_incident(KecleonDiscovery *s,SilphManagedRef display,uint64_t wall,SilphGameStop *out){
    if(!display.serial)return 0;uint64_t type=0,done=0,start=0,end=0;int r=discovery_scalar(s,display,"IncidentDisplayType",SILPH_READ_ENUM32,&type);
    if(!r){s->displays++;if(type<32)s->types|=1u<<type;}if(r||type!=8)return r;
    if(!r)r=discovery_scalar(s,display,"IncidentCompleted",SILPH_READ_BOOL,&done);
    if(!r)r=discovery_scalar(s,display,"IncidentStartMs",SILPH_READ_I64,&start);
    if(!r)r=discovery_scalar(s,display,"IncidentExpirationMs",SILPH_READ_I64,&end);
    if(!r){s->completed+=!!done;s->future+=start>wall;s->expired+=end<=wall||end>INT64_MAX;}if(r||done||start>wall||end<=wall||end>INT64_MAX)return r;
    char id[161]={0};r=discovery_text(s,display,"IncidentId",id);if(!r&&(!id[0]||(out->active&&strcmp(out->incident,id))))return SILPH_MANAGED_CONTRACT;
    if(!r){out->incident_type=8;out->active=1;out->expires_ms=end;memcpy(out->incident,id,sizeof(out->incident));}return r;
}
static int discovery_fort(KecleonDiscovery *s,SilphManagedRef fort,uint64_t wall,SilphGameStop *out){
    SilphManagedRef display={0},list={0},item={0};unsigned n=0;int r=discovery_ref(s,fort,"PokestopDisplay",&display);
    if(!r)r=discovery_incident(s,display,wall,out);
    if(!r)r=discovery_ref(s,fort,"PokestopDisplays",&list);
    if(!r)r=discovery_count(s,list,64,&n);
    for(unsigned i=0;!r&&i<n;i++){r=discovery_item(s,list,i,&item);if(!r)r=discovery_incident(s,item,wall,out);int d=discovery_drop(s,&item);if(!r)r=d;}
    SilphManagedRef *refs[]={&item,&list,&display};for(unsigned i=0;i<3;i++){int d=discovery_drop(s,refs[i]);if(!r)r=d;}return r;
}
static int discovery_parse(KecleonDiscovery *s,SilphManagedRef response,const SilphKecleonCandidate *candidate,double lat,double lon,uint64_t wall,SilphGameStop *out){
    *out=(SilphGameStop){0};s->displays=s->types=s->future=s->expired=s->completed=0;SilphManagedRef cells={0},cell={0},forts={0},fort={0};unsigned n=0,total=0,matches=0,scanned=0;char seen[8][161]={{0}};
    int r=discovery_ref(s,response,"MapCell",&cells);if(!r)r=discovery_count(s,cells,128,&n);
    for(unsigned i=0;!r&&i<n;i++){
        r=discovery_item(s,cells,i,&cell);if(!r)r=discovery_ref(s,cell,"Fort",&forts);unsigned count=0;if(!r)r=discovery_count(s,forts,768,&count);total+=count;if(total>768)r=SILPH_MANAGED_CAPACITY;
        for(unsigned j=0;!r&&j<count;j++){
            r=discovery_item(s,forts,j,&fort);uint64_t a=0,b=0;double x=0,y=0;
            if(!r)r=discovery_scalar(s,fort,"Latitude",SILPH_READ_F64,&a);if(!r)r=discovery_scalar(s,fort,"Longitude",SILPH_READ_F64,&b);memcpy(&x,&a,8);memcpy(&y,&b,8);scanned++;
            if(!r&&(!isfinite(x)||!isfinite(y)||fabs(x)>90||fabs(y)>180))r=SILPH_MANAGED_CONTRACT;
            if(!r&&discovery_distance(x,y,candidate->latitude,candidate->longitude)<=3&&discovery_distance(x,y,lat,lon)<=50){
                uint64_t type=0,enabled=0,closed=0;r=discovery_scalar(s,fort,"FortType",SILPH_READ_ENUM32,&type);if(!r)r=discovery_scalar(s,fort,"Enabled",SILPH_READ_BOOL,&enabled);if(!r)r=discovery_scalar(s,fort,"Closed",SILPH_READ_BOOL,&closed);
                if(!r&&type==1&&enabled&&!closed){
                    SilphGameStop row={.latitude=x,.longitude=y,.player_latitude=lat,.player_longitude=lon,.meters=discovery_distance(x,y,lat,lon)};r=discovery_text(s,fort,"FortId",row.fort);bool duplicate=false;
                    for(unsigned k=0;k<matches;k++)if(!strcmp(seen[k],row.fort))duplicate=true;
                    if(!r&&!duplicate){if(matches>=8)r=SILPH_MANAGED_CAPACITY;else{memcpy(seen[matches++],row.fort,sizeof(row.fort));r=discovery_fort(s,fort,wall,&row);if(!r&&matches==1)*out=row;}}
                }
            }
            int d=discovery_drop(s,&fort);if(!r)r=d;
        }
        int a=discovery_drop(s,&forts),b=discovery_drop(s,&cell);if(!r)r=a?a:b;
    }
    SilphManagedRef *refs[]={&fort,&forts,&cell,&cells};for(unsigned i=0;i<4;i++){int d=discovery_drop(s,refs[i]);if(!r)r=d;}
    if(r)*out=(SilphGameStop){0};else{out->scanned=scanned;out->matches=matches;out->scan_complete=1;}return r;
}
static int discovery_close(KecleonDiscovery *s){if(!s->rpc)return 0;int r=silph_rpc_action_close(s->rpc);if(!r){free(s->rpc);s->rpc=NULL;s->pending=0;}return r;}
static int discovery_reset(KecleonDiscovery *s){int r=s->rpc?silph_rpc_action_reset(s->rpc):0;if(!r)s->pending=0;return r;}
static void discovery_negative(KecleonDiscovery *s,uint64_t now){
    if(s->stop.scan_complete&&s->stop.matches==1&&s->stop.fort[0]&&!s->stop.active&&(s->types&(1u<<9))&&!(s->types&(1u<<8))){
        if(strcmp(s->negative_fort,s->stop.fort)||!s->negative_count||now<s->negative_at||now-s->negative_at>UINT64_C(20000000000))s->negative_count=1;
        else if(now-s->negative_at>=UINT64_C(2000000000)&&s->negative_count<2)s->negative_count++;
        memcpy(s->negative_fort,s->stop.fort,sizeof(s->negative_fort));s->negative_at=now;
    }else{s->negative_count=0;s->negative_fort[0]=0;}
}
static bool discovery_cached(KecleonDiscovery *s,const SilphKecleonPlanState *p,uint64_t now,uint64_t wall,SilphGameStop *out){
    if(s->request!=p->request||s->move!=p->move_id||!s->received||now<s->received||now-s->received>UINT64_C(20000000000)||!s->stop.scan_complete||s->stop.matches!=1||s->stop.incident_type!=8||!s->stop.active||s->stop.complete||s->stop.expires_ms<=wall)return false;
    double lat=0,lon=0;if(silph_game_read_position(s->game,&lat,&lon))return false;double meters=discovery_distance(lat,lon,s->stop.latitude,s->stop.longitude);if(meters>50||discovery_distance(s->stop.latitude,s->stop.longitude,p->candidate.latitude,p->candidate.longitude)>3)return false;
    *out=s->stop;out->player_latitude=lat;out->player_longitude=lon;out->meters=meters;return true;
}
static int discovery_tick(KecleonDiscovery *s,void *read,void *managed,void *game,const SilphScheduleIdentity *identity,const SilphKecleonPlanState *p,const SilphSchedulePermit *permit,uint64_t now,uint64_t wall){
    s->read=read;s->managed=managed;s->game=game;
    if(s->request!=p->request||s->move!=p->move_id){int r=discovery_reset(s);if(r)return r;if(s->request!=p->request){s->negative_count=0;s->negative_fort[0]=0;}s->request=p->request;s->move=p->move_id;s->stop=(SilphGameStop){0};s->received=0;}
    if(p->phase!=SILPH_KPLAN_SCAN)return s->pending?discovery_reset(s):0;
    if(s->disabled)return 0;
    double lat=0,lon=0;if(silph_game_read_position(game,&lat,&lon)||discovery_distance(lat,lon,p->candidate.latitude,p->candidate.longitude)>50)return 0;
    if(!s->rpc){s->rpc=calloc(1,silph_rpc_action_size());if(!s->rpc)return SILPH_MANAGED_CAPACITY;s->identity=*identity;int r=silph_rpc_action_open(s->rpc,silph_rpc_action_size(),read,managed,game,identity);if(r){free(s->rpc);s->rpc=NULL;s->disabled=1;__android_log_print(ANDROID_LOG_INFO,"SilphKecleon","discovery fallback open=%d",r);return 0;}}
    if(s->pending){
        if(now<s->poll_after)return 0;s->poll_after=now+UINT64_C(100000000);SilphPromiseMetrics metrics={0};int r=silph_rpc_action_poll(s->rpc,&s->identity,now,&metrics);if(!r&&metrics.state==SILPH_PROMISE_WAITING&&now-s->sent<UINT64_C(8000000000))return 0;
        SilphManagedRef result={0};bool fresh=false;uint64_t status=0;
        if(!r&&metrics.state==SILPH_PROMISE_RESOLVED)r=silph_rpc_action_take(s->rpc,&s->identity,now,&result,&fresh);else if(!r)r=SILPH_MANAGED_NOT_READY;
        if(!r&&!fresh)r=SILPH_MANAGED_STALE;if(!r)r=discovery_scalar(s,result,"Status",SILPH_READ_ENUM32,&status);
        if(!r&&status==1){r=discovery_parse(s,result,&p->candidate,lat,lon,wall,&s->stop);if(!r){s->received=now;s->failures=0;discovery_negative(s,now);}}
        else if(!r)s->failures++;
        int dropped=discovery_drop(s,&result),reset=discovery_reset(s);if(dropped||reset)return dropped?dropped:reset;
        if(r==SILPH_MANAGED_CONTRACT||r==SILPH_MANAGED_CAPACITY)s->disabled=1;
        if(r)s->failures++;s->next_query=now+(s->failures?UINT64_C(6000000000):UINT64_C(3200000000));
        __android_log_print(ANDROID_LOG_INFO,"SilphKecleon","discovery request=%llu status=%llu error=%d forts=%u matches=%u active=%u displays=%u types=%x future=%u expired=%u completed=%u queryMs=%llu fallback=%u",(unsigned long long)p->request,(unsigned long long)status,r,s->stop.scanned,s->stop.matches,s->stop.active,s->displays,s->types,s->future,s->expired,s->completed,(unsigned long long)((now-s->sent)/1000000),s->disabled);return 0;
    }
    if(now<s->next_query)return 0;
    SilphRequestInput input={.abi=1,.latitude=p->candidate.latitude,.longitude=p->candidate.longitude,.player_latitude=lat,.player_longitude=lon};int r=silph_rpc_action_send(s->rpc,permit,SILPH_RPC_DISCOVERY_MAP,&input,p->request,now);
    if(!r){s->pending=1;s->sent=now;s->poll_after=now+UINT64_C(100000000);}
    else{int reset=discovery_reset(s);if(reset)return reset;s->disabled=1;__android_log_print(ANDROID_LOG_INFO,"SilphKecleon","discovery fallback request=%llu error=%d",(unsigned long long)p->request,r);}return 0;
}
