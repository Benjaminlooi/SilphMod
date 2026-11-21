#define _GNU_SOURCE
#include "hunting.h"
#include "hunting-predictor.h"
#include "managed-proto.h"
#include "rpc-actions.h"
#include "promise.h"
#include "response-rewards.h"
#include <shadowhook.h>
#include <android/log.h>
#include <stdatomic.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <time.h>

int silph_hunting_coordinates(void *,const char *,double *,double *);

typedef struct {
    const void **(*assemblies)(void *,size_t *);
    const void *(*image)(const void *);
    const char *(*image_name)(const void *);
    void *(*klass)(const void *,const char *,const char *);
    const void *(*parameter)(const void *,uint32_t);
    void *(*class_from_type)(const void *);
    const void *(*field)(void *,const char *),*(*field_type)(const void *),*(*get_method)(void *,const char *,int);
    uint32_t (*field_flags)(const void *);
    void (*get)(void *,const void *,void *);
    bool (*is_enum)(void *),(*value_type)(void *);
    const char *(*class_name)(void *);
    const void *(*enum_base)(void *);
    const uint16_t *(*chars)(void *);
} HApi;
typedef struct {uint64_t id,seen;uint32_t species,form;char spawn[17];SilphPrediction prediction;double latitude,longitude;unsigned located;} Appearance;
typedef struct {
    SilphManagedApi base;HApi api;SilphManagedResolver resolve;void *context;
    atomic_uint enabled,inflight,suppressed;atomic_uint_fast64_t map_count,map_errors,map_background_skipped,background_resume_until;
    pthread_mutex_t cache_lock;Appearance cache[512];unsigned cursor;
    uint64_t checked_ids[4096];unsigned checked_count,checked_cursor;
    void *read,*managed,*game,*rpc,*map_hook;
    SilphScheduleIdentity identity;SilphSessionConfig config;SilphKecleonEvent event;
    uint64_t request,event_sequence,deadline,scan_started,refresh_at,authorize_until,next_action,next_scan,map_sent,map_spacing,target_expires;
    uint64_t caught,fled,encounters,throws,berries,skipped,target_id,captured_id,pending_at,xp,stardust;
    uint32_t reward_error;
    uint32_t phase,reason,expected_species,expected_cp,expected_level,pending,operation,item,active_berry,berry_attempted,berry_suppressed,ready,map_attempts,map_failures,map_successes,verified,dirty,stopped;
    uint32_t balls[4],ball_count,target_throws,budget_ms;double latitude,longitude;char source[97],spawn[17];
} Hunting;
static Hunting state;
static void (*map_original)(void *,bool,const void *);
static uint64_t stamp(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return(uint64_t)t.tv_sec*UINT64_C(1000000000)+t.tv_nsec;}
static double distance(double a,double b,double c,double d){double p=0.017453292519943295,h=pow(sin((c-a)*p/2),2)+cos(a*p)*cos(c*p)*pow(sin((d-b)*p/2),2);return 12742017.6*asin(sqrt(fmin(1,fmax(0,h))));}
static bool get(void *object,const char *name,int kind,void *out){
    Hunting *s=&state;if(!object)return false;const void *f=s->api.field(s->base.object_get_class(object),name);if(!f||(s->api.field_flags(f)&0x10))return false;
    const void *t=s->api.field_type(f);int actual=s->base.type_get_type(t);void *k=s->api.class_from_type(t);if(kind==8&&k&&s->api.is_enum(k))actual=s->base.type_get_type(s->api.enum_base(k));
    if(kind==21){const char *n=k?s->api.class_name(k):NULL;if((actual!=18&&actual!=21)||!n||strncmp(n,"RepeatedField",13)||s->api.value_type(k))return false;}
    else if(actual!=kind)return false;s->api.get(object,f,out);return true;
}
static bool token(void *wild,char out[17]){
    Hunting *s=&state;void *text=NULL;if(!get(wild,"SpawnPointId",14,&text)||!text)return false;int32_t n=s->base.string_length(text);const uint16_t *v=s->api.chars(text);
    if(!v||n<1||n>16)return false;memset(out,0,17);for(int32_t i=0;i<n;i++){if(!((v[i]>='0'&&v[i]<='9')||(v[i]>='a'&&v[i]<='f')))return false;out[i]=(char)v[i];}return true;
}
static int count(void *list){
    Hunting *s=&state;if(!list)return 0;void *k=s->base.object_get_class(list);const void *m=s->api.get_method(k,"get_Count",0);void *e=NULL,*box=m?s->base.runtime_invoke(m,list,NULL,&e):NULL;int32_t n=0;
    if(e||!box)return 0;void *v=s->base.object_unbox(box);if(!v)return 0;memcpy(&n,v,4);return n>=0&&n<=4096?n:0;
}
static void *item(void *list,int i){Hunting *s=&state;const void *m=s->api.get_method(s->base.object_get_class(list),"get_Item",1);void *e=NULL,*args[]={&i};void *r=m?s->base.runtime_invoke(m,list,args,&e):NULL;return e?NULL:r;}
static void observe(void *message){
    Hunting *s=&state;uint32_t status=0;if(!get(message,"Status",8,&status))return;atomic_fetch_add(&s->map_count,1);if(status!=1){atomic_fetch_add(&s->map_errors,1);return;}
    uintptr_t root=s->base.gchandle_new(message,false);if(!root)return;unsigned wild_count=0,parsed=0;void *cells=NULL;if(get(message,"MapCell",21,&cells)){
        int n=count(cells);for(int i=0;i<n;i++){void *cell=item(cells,i),*wilds=NULL;if(!cell||!get(cell,"WildPokemon",21,&wilds))continue;
            int total=count(wilds);wild_count+=(unsigned)total;for(int j=0;j<total;j++){void *wild=item(wilds,j),*pokemon=NULL,*display=NULL;Appearance a={.seen=stamp()};uint32_t weather=0;uint8_t strong=0;
                if(!wild||!get(wild,"EncounterId",11,&a.id)||!a.id||!token(wild,a.spawn)||!get(wild,"Pokemon",18,&pokemon)||!pokemon||!get(pokemon,"pokemonId_",8,&a.species)||!get(pokemon,"pokemonDisplay_",18,&display)||!display
                    ||!get(display,"Form",8,&a.form)||!get(display,"WeatherBoostedCondition",8,&weather)||!get(display,"IsStrongPokemon",2,&strong)||strong>1)continue;
                a.located=get(wild,"Latitude",13,&a.latitude)&&get(wild,"Longitude",13,&a.longitude)&&isfinite(a.latitude)&&isfinite(a.longitude)&&fabs(a.latitude)<=90&&fabs(a.longitude)<=180&&(a.latitude!=0||a.longitude!=0);
                parsed++;a.prediction=silph_predict(a.id,weather!=0,strong!=0);pthread_mutex_lock(&s->cache_lock);unsigned at=512;for(unsigned k=0;k<512;k++)if(s->cache[k].id==a.id){at=k;break;}if(at==512)at=s->cursor++%512;else if(!a.located&&s->cache[at].located){a.located=1;a.latitude=s->cache[at].latitude;a.longitude=s->cache[at].longitude;}s->cache[at]=a;pthread_mutex_unlock(&s->cache_lock);
            }
        }
    }s->base.gchandle_free(root);__android_log_print(ANDROID_LOG_INFO,"SilphHunting","map wild=%u parsed=%u",wild_count,parsed);
}
static void map_query_hook(void *self,bool force,const void *method){
    Hunting *s=&state;atomic_fetch_add(&s->inflight,1);if(atomic_load(&s->enabled)&&atomic_load(&s->suppressed)&&stamp()>=atomic_load(&s->background_resume_until))atomic_fetch_add(&s->map_background_skipped,1);else map_original(self,force,method);atomic_fetch_sub(&s->inflight,1);
}
static void suppress_background(Hunting *s,uint32_t offset,SilphVisualAddress valid){
    size_t count=0;const void **all=s->api.assemblies(s->base.domain_get(),&count);void *klass=NULL;if(!all||count>2048)return;
    for(size_t i=0;i<count;i++){const void *image=s->api.image(all[i]);const char *name=image?s->api.image_name(image):NULL;if(name&&!strcmp(name,"holo-game.dll")){klass=s->api.klass(image,"Niantic.Holoholo.Map","MapContentHandler");break;}}
    const void *method=klass?s->api.get_method(klass,"cmzw",1):NULL;uint32_t flags=0;if(!method||(s->base.method_get_flags(method,&flags)&0x10)||s->base.method_is_generic(method)||s->base.method_get_param_count(method)!=1||s->base.type_get_type(s->base.method_get_return_type(method))!=1||s->base.type_get_type(s->api.parameter(method,0))!=2)return;
    uintptr_t address=0;memcpy(&address,(const char *)method+offset,8);if((address&3)||!valid(address,s->context))return;s->map_hook=shadowhook_hook_func_addr((void *)address,(void *)map_query_hook,(void **)&map_original);
}
int silph_hunting_start(const SilphManagedApi *base,SilphManagedResolver resolve,void *context,uint32_t offset,SilphVisualAddress valid){
    Hunting *s=&state;if(atomic_load(&s->enabled))return 0;s->base=*base;s->resolve=resolve;s->context=context;pthread_mutex_init(&s->cache_lock,NULL);atomic_init(&s->enabled,0);atomic_init(&s->inflight,0);atomic_init(&s->suppressed,0);atomic_init(&s->map_count,0);atomic_init(&s->map_errors,0);atomic_init(&s->map_background_skipped,0);
#define LOAD(member,name) do{*(void **)(&s->api.member)=resolve("il2cpp_" name,context);if(!s->api.member)return 2;}while(0)
    LOAD(assemblies,"domain_get_assemblies");LOAD(image,"assembly_get_image");LOAD(image_name,"image_get_name");LOAD(klass,"class_from_name");LOAD(parameter,"method_get_param");LOAD(class_from_type,"class_from_type");LOAD(field,"class_get_field_from_name");LOAD(field_type,"field_get_type");LOAD(get_method,"class_get_method_from_name");LOAD(field_flags,"field_get_flags");LOAD(get,"field_get_value");LOAD(is_enum,"class_is_enum");LOAD(value_type,"class_is_valuetype");LOAD(class_name,"class_get_name");LOAD(enum_base,"class_enum_basetype");LOAD(chars,"string_chars");
#undef LOAD
    suppress_background(s,offset,valid);atomic_store(&s->enabled,1);__android_log_print(ANDROID_LOG_INFO,"SilphHunting","collector ready directMap=true cache=512 backgroundMapManaged=%u",s->map_hook!=NULL);return 0;
}
static void event(Hunting *s){s->dirty=1;}
static bool checked(Hunting *s,uint64_t id){for(unsigned i=0;i<s->checked_count;i++)if(s->checked_ids[i]==id)return true;return false;}
static void finish(Hunting *s,uint32_t reason){if(s->target_id&&(reason==1||reason==2||reason==6||reason==7)&&!checked(s,s->target_id)){s->checked_ids[s->checked_cursor++%4096]=s->target_id;if(s->checked_count<4096)s->checked_count++;}s->phase=7;s->reason=reason;s->verified=0;event(s);__android_log_print(ANDROID_LOG_INFO,"SilphHunting","result request=%llu reason=%u id=%llu maps=%llu errors=%llu",(unsigned long long)s->request,reason,(unsigned long long)s->target_id,(unsigned long long)atomic_load(&s->map_count),(unsigned long long)atomic_load(&s->map_errors));}
static int release(Hunting *s,SilphManagedRef *ref){if(!ref->serial)return 0;int r=silph_managed_release(s->managed,*ref);if(!r)*ref=(SilphManagedRef){0};return r;}
static int refresh(Hunting *s,const SilphSchedulePermit *permit,uint64_t now){
    if(now<s->refresh_at||now<s->map_sent+s->map_spacing||s->map_attempts>=4)return 0;
    double lat=0,lon=0;int r=silph_game_read_position(s->game,&lat,&lon);if(r||distance(lat,lon,s->latitude,s->longitude)>40)return 0;
    SilphRequestInput input={.abi=1,.latitude=s->latitude,.longitude=s->longitude,.player_latitude=lat,.player_longitude=lon};r=silph_rpc_action_send(s->rpc,permit,15,&input,s->request,now);s->map_attempts++;s->map_sent=now;s->refresh_at=now+UINT64_C(2000000000);
    if(!r){s->pending=1;s->operation=15;s->pending_at=now;__android_log_print(ANDROID_LOG_INFO,"SilphHunting","map request=%llu attempt=%u cellLevel=15",(unsigned long long)s->request,s->map_attempts);}
    else{int reset=silph_rpc_action_reset(s->rpc);if(reset)return reset;finish(s,4);__android_log_print(ANDROID_LOG_INFO,"SilphHunting","map request deferred error=%d",r);}return 0;
}
static int target(Hunting *s,const SilphKecleonCommand *c,uint64_t now){
    unsigned species=0,cp=0,level=0,budget=0,ttl=0,b[4]={0};int used=0;
    if(sscanf(c->source,"%u:%u:%u:%u:%u:%u.%u.%u.%u%n",&species,&cp,&level,&budget,&ttl,&b[0],&b[1],&b[2],&b[3],&used)!=9||c->source[used]||species<1||species>=2048||cp<1||cp>20000||level<1||level>55||budget<8000||budget>90000||ttl<3000||ttl>3600000)return SILPH_MANAGED_ARGUMENT;
    unsigned count=0;for(unsigned i=0;i<4;i++){if(!b[i])continue;if((b[i]<1||b[i]>3)&&b[i]!=7)return SILPH_MANAGED_ARGUMENT;for(unsigned j=0;j<i;j++)if(b[j]==b[i])return SILPH_MANAGED_ARGUMENT;b[count++]=b[i];}if(!count)return SILPH_MANAGED_ARGUMENT;
    s->latitude=c->latitude;s->longitude=c->longitude;s->expected_species=species;s->expected_cp=cp;s->expected_level=level;s->budget_ms=budget;s->target_expires=now+(uint64_t)ttl*1000000;s->deadline=now+(uint64_t)(budget<ttl?budget:ttl)*1000000;s->scan_started=s->next_scan=0;s->map_attempts=s->map_failures=0;s->target_id=0;s->captured_id=0;s->verified=0;s->active_berry=s->berry_attempted=s->berry_suppressed=s->target_throws=0;s->authorize_until=0;s->reason=0;s->ball_count=count;memcpy(s->balls,b,sizeof(b));memcpy(s->source,c->source,sizeof(s->source));memset(s->spawn,0,sizeof(s->spawn));
    s->phase=2;event(s);__android_log_print(ANDROID_LOG_INFO,"SilphHunting","target request=%llu species=%u budgetMs=%u ttlMs=%u",(unsigned long long)s->request,species,budget,ttl);return 0;
}
static int send(Hunting *s,const SilphSchedulePermit *permit,unsigned operation,uint64_t now){
    if(now>=s->target_expires){finish(s,3);return 0;}
    double lat=0,lon=0;int r=silph_game_read_position(s->game,&lat,&lon);if(r||distance(lat,lon,s->latitude,s->longitude)>40){finish(s,8);return 0;}
    if(operation!=13&&(!s->verified||now>=s->authorize_until)){s->phase=4;event(s);return 0;}
    SilphRequestInput input={.abi=1,.encounter=s->target_id,.item=(int32_t)s->item,.quality=s->config.quality,.curve=!!(s->config.flags&SILPH_CFG_CURVE),.player_latitude=lat,.player_longitude=lon};memcpy(input.location,s->spawn,17);
    r=silph_rpc_action_send(s->rpc,permit,operation,&input,s->target_id,now);if(!r){s->pending=1;s->operation=operation;s->pending_at=now;if(operation==14){s->throws++;s->target_throws++;}else if(operation==3)s->berries++;event(s);__android_log_print(ANDROID_LOG_INFO,"SilphHunting","rpc request=%llu operation=%u id=%llu item=%u",(unsigned long long)s->request,operation,(unsigned long long)s->target_id,s->item);}
    else{int reset=silph_rpc_action_reset(s->rpc);if(reset)return reset;finish(s,4);__android_log_print(ANDROID_LOG_INFO,"SilphHunting","rpc deferred error=%d",r);}return 0;
}
static int resolved(Hunting *s,SilphManagedRef result,uint64_t now){
    void *body_root=NULL;uint32_t status_code=0;int r=silph_managed_target(s->managed,result,&body_root);if(r)return r;if(!get(body_root,"Status",8,&status_code))return SILPH_MANAGED_CONTRACT;uint64_t code=status_code;
    if(s->operation==15){
        observe(body_root);if(code==1){s->map_failures=0;if(++s->map_successes>=8){s->map_successes=0;if(s->map_spacing>UINT64_C(3400000000))s->map_spacing-=UINT64_C(200000000);}}
        else{if(s->map_failures<3)s->map_failures++;s->map_successes=0;if(s->map_spacing<UINT64_C(6000000000))s->map_spacing+=UINT64_C(1000000000);if(s->map_failures>=3){atomic_store(&s->background_resume_until,now+UINT64_C(10000000000));__android_log_print(ANDROID_LOG_INFO,"SilphHunting","map synchronization request=%llu graceMs=10000",(unsigned long long)s->request);}}
        s->refresh_at=now+(UINT64_C(2000000000)<<(s->map_failures>2?2:s->map_failures));
        void *cells=NULL;int cells_count=get(body_root,"MapCell",21,&cells)?count(cells):0;
        __android_log_print(ANDROID_LOG_INFO,"SilphHunting","map response request=%llu status=%u cells=%d queryMs=%llu spacingMs=%llu",(unsigned long long)s->request,status_code,cells_count,(unsigned long long)((now-s->pending_at)/1000000),(unsigned long long)(s->map_spacing/1000000));
    }else if(s->operation==13){
        s->encounters++;if(code!=1){finish(s,3);return 0;}
        void *body=NULL,*wild=NULL,*pokemon=NULL,*display=NULL;uint64_t id=0;uint32_t species=0,a=0,d=0,hp=0,cp=0;uint8_t shiny=0;
        r=silph_managed_target(s->managed,result,&body);
        if(!r&&(!get(body,"Pokemon",18,&wild)||!wild||!get(wild,"EncounterId",11,&id)||!get(wild,"Pokemon",18,&pokemon)||!pokemon
            ||!get(pokemon,"pokemonId_",8,&species)||!get(pokemon,"individualAttack_",8,&a)||!get(pokemon,"individualDefense_",8,&d)||!get(pokemon,"individualStamina_",8,&hp)||!get(pokemon,"cp_",8,&cp)
            ||!get(pokemon,"pokemonDisplay_",18,&display)||!display||!get(display,"Shiny",2,&shiny)))r=SILPH_MANAGED_CONTRACT;
        if(r)return r;__android_log_print(ANDROID_LOG_INFO,"SilphHunting","checked request=%llu id=%llu species=%llu cp=%llu iv=%llu/%llu/%llu shiny=%llu queryMs=%llu",(unsigned long long)s->request,(unsigned long long)id,(unsigned long long)species,(unsigned long long)cp,(unsigned long long)a,(unsigned long long)d,(unsigned long long)hp,(unsigned long long)shiny,(unsigned long long)((now-s->pending_at)/1000000));
        if(id!=s->target_id||species!=s->expected_species){finish(s,4);return 0;}if(a!=15||d!=15||hp!=15||shiny!=1){s->skipped++;finish(s,2);return 0;}
        s->verified=1;s->phase=4;s->reason=0;event(s);
    }else if(s->operation==14){
        if(code==1){uint64_t id=0;r=get(body_root,"CapturedPokemonId",11,&id)?0:SILPH_MANAGED_CONTRACT;if(r||!id)return r?r:SILPH_MANAGED_CONTRACT;s->captured_id=id;s->caught++;uint64_t xp=0,dust=0;int rewards=silph_capture_rewards(s->read,s->managed,result,&xp,&dust);if(!rewards){s->xp+=xp;s->stardust+=dust;}else s->reward_error=1;finish(s,1);}
        else if(code==2||code==4){s->active_berry=0;s->berry_attempted=s->berry_suppressed=0;s->next_action=now+UINT64_C(100000000);event(s);}
        else if(code==3){s->fled++;finish(s,6);}else finish(s,4);
    }else{uint32_t active=0;r=get(body_root,"ActiveItem",8,&active)?0:SILPH_MANAGED_CONTRACT;if(r)return r;if((code==0||code==2)&&active)s->active_berry=(uint32_t)active;else s->berry_suppressed=1;if(code==1)finish(s,3);else s->next_action=now+UINT64_C(100000000);}
    return 0;
}
int silph_hunting_tick(void *read,void *managed,void *game,void *link,const SilphScheduleIdentity *identity,const SilphSessionConfig *config,const SilphSchedulePermit *permit,const SilphGameSnapshot *snapshot,uint64_t now){
    Hunting *s=&state;if(!atomic_load(&s->enabled))return SILPH_MANAGED_STATE;s->read=read;s->managed=managed;s->game=game;
    if(!s->ready){s->identity=*identity;s->identity.account_hi=snapshot->account_hi;s->identity.account_lo=snapshot->account_lo;if(!s->identity.account_hi&&!s->identity.account_lo)return 0;s->config=*config;
        int r=silph_proto_enable(read,s->resolve,s->context);if(!r)r=silph_rpc_enable(read,s->resolve,s->context);s->rpc=calloc(1,silph_rpc_action_size());if(!s->rpc)return SILPH_MANAGED_CAPACITY;if(!r)r=silph_rpc_action_open(s->rpc,silph_rpc_action_size(),read,managed,game,&s->identity);if(r)return r;s->ready=1;s->map_spacing=UINT64_C(3200000000);atomic_store(&s->suppressed,1);s->request=1;s->phase=1;event(s);}
    if(snapshot->account_hi!=s->identity.account_hi||snapshot->account_lo!=s->identity.account_lo){s->phase=8;s->reason=9;event(s);return 0;}
    SilphKecleonCommand c={0};int r=silph_session_link_pop(link,&c);
    if(!r&&c.request==s->request){if(c.kind==1&&s->phase==1){r=target(s,&c,now);if(r){__android_log_print(ANDROID_LOG_INFO,"SilphHunting","target rejected error=%d",r);finish(s,8);}}
        else if(c.kind==2&&s->phase==2){if(c.accepted){s->phase=3;s->scan_started=now;s->deadline=now+(uint64_t)s->budget_ms*1000000;if(s->deadline>s->target_expires)s->deadline=s->target_expires;s->refresh_at=now+UINT64_C(200000000);event(s);}else finish(s,8);}
        else if(c.kind==3&&s->phase==4){if(c.accepted){s->phase=5;s->authorize_until=now+UINT64_C(20000000000);s->next_action=now;event(s);}else finish(s,2);}
        else if(c.kind==4&&s->phase==7){s->request++;s->phase=1;s->reason=s->verified=0;memset(s->source,0,sizeof(s->source));memset(s->spawn,0,sizeof(s->spawn));s->target_id=0;if(config->max_catches&&s->caught>=config->max_catches){s->phase=8;s->reason=5;}event(s);}
    }
    if(s->pending&&s->operation==15&&s->phase==3&&now>=s->deadline){r=silph_rpc_action_reset(s->rpc);s->pending=0;if(r)return r;finish(s,3);return 0;}
    if(s->pending){SilphPromiseMetrics metrics={0};r=silph_rpc_action_poll(s->rpc,&s->identity,now,&metrics);if(!r&&metrics.state==SILPH_PROMISE_RESOLVED){SilphManagedRef result={0};bool fresh=false;r=silph_rpc_action_take(s->rpc,&s->identity,now,&result,&fresh);if(!r&&fresh)r=resolved(s,result,now);else if(!r)r=SILPH_MANAGED_STALE;release(s,&result);int reset=silph_rpc_action_reset(s->rpc);s->pending=0;if(!r)r=reset;if(r){finish(s,4);__android_log_print(ANDROID_LOG_INFO,"SilphHunting","response deferred error=%d",r);}}
        else if(r||metrics.state!=SILPH_PROMISE_WAITING){int reset=silph_rpc_action_reset(s->rpc);s->pending=0;finish(s,4);if(reset)return reset;}return 0;}
    if(s->phase==3){if(now<s->next_scan)return 0;s->next_scan=now+UINT64_C(100000000);double lat=0,lon=0;if(silph_game_read_position(game,&lat,&lon)||distance(lat,lon,s->latitude,s->longitude)>40){if(now>=s->deadline)finish(s,8);return 0;}
        Appearance found={0},duplicate={0},options[32];unsigned count=0;bool nonhundo=false;pthread_mutex_lock(&s->cache_lock);for(unsigned pass=0;pass<2;pass++)for(unsigned i=0;i<512&&count<32;i++){Appearance *a=&s->cache[i];bool hundo=a->prediction.attack==15&&a->prediction.defense==15&&a->prediction.stamina==15;if((pass==0)!=hundo)continue;if(a->id&&a->species==s->expected_species&&now>=a->seen&&now-a->seen<=UINT64_C(90000000000))options[count++]=*a;}pthread_mutex_unlock(&s->cache_lock);
        double best=DBL_MAX;for(unsigned i=0;i<count;i++){Appearance a=options[i];if(!a.located){int located=silph_hunting_coordinates(read,a.spawn,&a.latitude,&a.longitude);if(located)continue;a.located=1;pthread_mutex_lock(&s->cache_lock);for(unsigned j=0;j<512;j++)if(s->cache[j].id==a.id){s->cache[j].located=1;s->cache[j].latitude=a.latitude;s->cache[j].longitude=a.longitude;break;}pthread_mutex_unlock(&s->cache_lock);}
            double meters=distance(a.latitude,a.longitude,s->latitude,s->longitude);if(meters>40)continue;if(checked(s,a.id)){duplicate=a;continue;}bool hundo=a.prediction.attack==15&&a.prediction.defense==15&&a.prediction.stamina==15;if(!hundo){nonhundo=true;if(now-s->scan_started<UINT64_C(5000000000))continue;}
            double score=meters+(hundo?0:1000)+(a.prediction.level==s->expected_level?0:100);if(score<best){best=score;found=a;}}
        if(found.id){s->target_id=found.id;memcpy(s->spawn,found.spawn,17);__android_log_print(ANDROID_LOG_INFO,"SilphHunting","resolved request=%llu id=%llu species=%u level=%u offsetM=%.1f mapWaitMs=%llu",(unsigned long long)s->request,(unsigned long long)found.id,found.species,found.prediction.level,distance(found.latitude,found.longitude,s->latitude,s->longitude),(unsigned long long)((now-s->scan_started)/1000000));return send(s,permit,13,now);}
        if(duplicate.id&&duplicate.seen>=s->scan_started){s->target_id=duplicate.id;memcpy(s->spawn,duplicate.spawn,17);s->skipped++;finish(s,7);return 0;}
        if(now>=s->deadline||(s->map_attempts>=4&&now>=s->refresh_at)){finish(s,nonhundo?2:3);return 0;}return refresh(s,permit,now);
    }else if(s->phase==5&&now>=s->next_action){
        if(!s->verified||!s->target_id){finish(s,4);return 0;}if(snapshot->max_pokemon&&snapshot->pokemon_count>=snapshot->max_pokemon){s->phase=8;s->reason=8;event(s);return 0;}if(s->target_throws>=50){finish(s,4);return 0;}
        unsigned ball=0,berry=0;for(unsigned i=0;!ball&&i<s->ball_count;i++){uint32_t n=0;if(!silph_game_read_item(game,(int32_t)s->balls[i],&n)&&n)ball=s->balls[i];}
        if(!ball){s->phase=8;s->reason=8;event(s);return 0;}
        if((config->flags&SILPH_CFG_BERRIES)&&!s->active_berry&&!s->berry_attempted&&!s->berry_suppressed)for(unsigned i=0;!berry&&i<config->berry_count;i++){uint32_t n=0;if(!silph_game_read_item(game,(int32_t)config->berries[i],&n)&&n)berry=config->berries[i];}
        s->item=berry?berry:ball;if(berry)s->berry_attempted=1;return send(s,permit,berry?3:14,now);
    }
    return 0;
}
void silph_hunting_snapshot(SilphSessionSnapshot *out){Hunting *s=&state;out->caught=s->caught;out->fled=s->fled;out->encounters=s->encounters;out->shinies=s->caught;out->berries=s->berries;out->throws=s->throws;out->encounter=s->target_id;out->captured=s->captured_id;out->candidate_request=s->request;out->candidate_state=s->phase;out->reason=s->reason;out->discarded=s->skipped;out->latitude=s->latitude;out->longitude=s->longitude;out->xp=s->xp;out->stardust=s->stardust;out->rewards_available=s->reward_error?0:3;}
int silph_hunting_publish(void *link){
    Hunting *s=&state;if(!s->ready||!s->dirty)return 0;SilphKecleonEvent e={.identity=s->identity,.sequence=s->event_sequence+1,.request=s->request,.phase=s->phase,.reason=s->reason,.verified=s->verified,.latitude=s->latitude,.longitude=s->longitude};
    if(e.phase==1)silph_game_read_position(s->game,&e.latitude,&e.longitude);memcpy(e.source,s->source,sizeof(e.source));if(s->spawn[0])memcpy(e.fort,s->spawn,17);if(s->target_id)snprintf(e.incident,sizeof(e.incident),"%llu",(unsigned long long)s->target_id);
    int r=silph_session_link_event(link,&e);if(!r){s->event_sequence=e.sequence;s->dirty=0;}return r==SILPH_SCHEDULE_BUSY?0:r;
}
int silph_hunting_close(void){
    Hunting *s=&state;atomic_store(&s->suppressed,0);atomic_store(&s->enabled,0);if(atomic_load(&s->inflight))return SILPH_MANAGED_NOT_READY;int r=0;if(s->rpc){r=silph_rpc_action_close(s->rpc);if(!r){free(s->rpc);s->rpc=NULL;}}if(r)return r;
    if(s->map_hook){if(shadowhook_unhook(s->map_hook))return SILPH_MANAGED_NOT_READY;s->map_hook=NULL;}
    __android_log_print(ANDROID_LOG_INFO,"SilphHunting","closed checked=%llu caught=%llu mapReplies=%llu errors=%llu backgroundSkipped=%llu",(unsigned long long)s->encounters,(unsigned long long)s->caught,(unsigned long long)atomic_load(&s->map_count),(unsigned long long)atomic_load(&s->map_errors),(unsigned long long)atomic_load(&s->map_background_skipped));return 0;
}
