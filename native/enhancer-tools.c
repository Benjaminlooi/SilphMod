#define _GNU_SOURCE
#include "enhancer-tools.h"
#include "callback-memory.h"
#include "response-rewards.h"
#include "core.h"
#include "farmer-actions.h"
#include "pokemon-selection.h"
#include "research-policy.h"
#include "rocket-bridge.h"
#include "collection-stats.h"
#ifdef SILPH_PILOT_AUTORAID
#include "raid-read.h"
#include "raid-pass.h"
#include "raid-policy.h"
#include "raid-clock-policy.h"
#include "raid-capture-policy.h"
#include "kecleon-wire.h"
#include "session-link.h"
typedef struct {void *klass;uint64_t key;const void *value;} RaidLookup;
typedef struct {
    int32_t (*graphics_get[4])(void);void (*graphics_set[4])(int32_t);int32_t graphics_saved[4],graphics_applied[4];unsigned graphics_mask;uint64_t graphics_at;
    SilphManagedRef render;const void *resolution_get,*resolution_set;float resolution_saved,resolution_applied;unsigned resolution_ready;
    SilphManagedRef vista;const void *vista_get[3];bool vista_saved[3];unsigned vista_mask,vista_checked;
    RaidLookup lookup[256];SilphManagedRef boss_proto;uint64_t boss_moves_next,restore_at;unsigned restore_attempts,boss_moves_ready;
    void *read,*managed,*game,*link,*reader,*pass,*map_pokemon_class;const void *capture;SilphScheduleIdentity identity;
    SilphManagedRef explore,root,gui,place,reward,map_handler,map_settings,factory,model,recovery_pending;
    const void *create_model;uint32_t group_cosmetics,headless,balls,initial_balls,ball,berry_mask,berry_confirmed,capture_authorized;uint64_t modal_log_at,scan_log_at;double scan_best_m;SilphRaidSite scan_best;uint64_t capture_throws,capture_berries,capture_at;double capture_latitude,capture_longitude;
    SilphRaidSite site;SilphRaidClock utc;uint64_t request,event_sequence,deadline,next,ui_at,expected,caught_before,fled_before,map_at,nudge_at,lobby_at,battle_ref,recovery_sync,rejoin_deadline,team[6];char wait_gym[161];
    SilphRaidSite survey_site;uint64_t survey_until;uint32_t surveying,survey_row,survey_done,survey_count,survey_error;
    uint32_t phase,stage,reason,dirty,scan,joined,ticket_started,ready,healing,recommended,won,filters,min_players,entry_gate;
    SilphManagedRef medicine_list;uint64_t medicine_at,medicine_until,medicine_scan;int32_t medicine_hp[18];
    unsigned medicine_count,medicine_item,medicine_mask,medicine_wait,medicine_rounds,medicine_active,medicine_disabled,medicine_limit;
    uint32_t species,level,form,costume,evolution,desired_pass,nudged,map_disabled,map_tuned,lobby_gate,wait_seconds,resolve_gate,resolve_status,reopen,resume,rejoins,rejoining,leave_reason,recovery_care,recovery_rpc;float map_saved[3];uint64_t expires;double latitude,longitude;char source[97],last_gui[128],resume_gym[161];uintptr_t acted_gui;
} AutoRaid;
#endif
#include <shadowhook.h>
#include <android/log.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdatomic.h>

typedef struct {
    const void **(*assemblies)(void *,size_t *);const void *(*image)(const void *);const char *(*image_name)(const void *);
    void *(*klass)(const void *,const char *,const char *),*(*parent)(void *),*(*nested)(void *,void **);
    void *(*object_new)(void *),*(*type_object)(const void *),*(*value_box)(void *,void *);const uint16_t *(*chars)(void *);
    const char *(*class_name)(void *);bool (*assignable)(void *,void *);
    const void *(*method)(void *,const char *,int),*(*parameter)(const void *,uint32_t),*(*field)(void *,const char *),*(*fields)(void *,void **),*(*field_type)(const void *);
    uint32_t (*field_flags)(const void *);const char *(*type_name)(const void *);void (*free)(void *);
    void (*get)(void *,const void *,void *),(*set)(void *,const void *,void *),(*set_object)(void *,const void *,void *);
    int32_t (*value_size)(void *,uint32_t *);void *(*class_from_type)(const void *);bool (*is_enum)(void *),(*is_value)(void *);const void *(*enum_base)(void *);
} EnhApi;
typedef struct {void *stub;uintptr_t address;unsigned char before[16];} EnhHook;
typedef struct {int32_t ball;float reticle;uint8_t bullseye,spinning,missed,pad;} EnhThrow;
_Static_assert(sizeof(EnhThrow)==12,"throw contract");
typedef struct {float x,y;} EnhPoint;
typedef struct {float x,y,z;} EnhVector;
typedef struct {double latitude,longitude;} EnhLocation;
typedef struct {SilphManagedRef object;float tween,intro;} EnhTiming;
typedef struct {SilphManagedRef text,original,rendered;bool rich;int32_t overflow;} EnhLabel;
typedef struct {int32_t id,start,end,duration,delta;float power;} EnhMove;
typedef struct {uint64_t id,encounter,at;uint32_t source,phase;} EnhTransfer;
typedef struct {uint64_t id,created,seed,retry;uint32_t ready,species,form,cp,level,fast,charged;SilphCollectionStats stats;char name[112],moves[2][80];} CollectionEntry;
typedef struct {EnhLabel label;char value[768];EnhPoint position;unsigned moved;} CollectionLabel;
typedef struct {SilphManagedRef view,text;} CollectionBadge;
typedef struct {
    void *adapter,*view,*panel,*reward,*data,*settings,*display,*move_enum,*master_type;
    const void *egg_create,*panel_update,*reward_update,*data_reward,*data_display,*data_name;
    SilphManagedRef master,player;CollectionEntry entries[32];CollectionLabel labels[64];unsigned cursor,label_count,eggs,rewards;
    const char *(*field_name)(const void *);void (*static_get)(const void *,void *);
    CollectionBadge badges[32];const void *instantiate,*destroy;unsigned layout_reported;
} CollectionView;
typedef struct {
    SilphManagedApi base;EnhApi api;SilphSessionConfig config;SilphEnhancerMetrics metrics;
    void *managed,*context,*allowed_context;SilphVisualAllowed allowed;SilphVisualAddress address;
    unsigned owner,closed,depth,offset,used,map,berry_tried,first_tick_logged;uint64_t last_id,promise_at,berry_at,tap_at;
    EnhPoint last_tap;EnhHook hooks[26];EnhTiming timings[12];unsigned timing_count,notice_sequence;
    SilphManagedRef nearby_gui;void *nearby_game,*nearby_scope,*nearby_entities_class;uint64_t nearby_at;
    void *interaction_class,*state_class,*map_class,*catch_class,*berry_class,*ground_class,*gesture_class,*camera_class,*ball_class,*transform_class;
    const void *state_get,*state_map,*state_complete,*state_shiny,*map_id,*active_berry,*set_berry,*can_berry,*ball_in_play;
    const void *ball_type,*ball_transform,*ball_reticle,*ball_body,*interaction_ball,*transform_position,*set_position,*set_velocity;
    const void *camera,*camera_tween,*camera_intro,*ground_gesture,*ground_controller,*gesture_position;
    SilphManagedRef interaction,state,promise,promise_state,berry_promise,berry_interaction;uint32_t pending_berry,shiny,promise_shiny;uint64_t promise_id,berry_id,throw_serial,berry_throw_serial;
    void *nameplate_class,*text_class,*pokemon_class,*raid_class,*director_class,*raid_manager_class,*event_class,*attack_class,*dodge_class;
    const void *unity_cached,*nameplate_text,*nameplate_pokemon,*nameplate_master,*text_get,*text_set,*rich_get,*rich_set,*overflow_get,*overflow_set,*level_at;
    const void *raid_director,*raid_manager,*raid_master,*submit_action;
    EnhLabel labels[12];unsigned label_count;float level_cp[61];unsigned level_count,level_ready;
    SilphManagedRef display_plate,display_map,raid_display_pokemon;uint64_t display_seen,display_target,display_after,display_until,raid_display_encounter;unsigned display_stage,display_reported;
    SilphManagedRef raid,raid_pending,raid_move_table,raid_move_iterator,raid_actor,raid_local;uint64_t raid_pending_at,raid_manual_at,raid_next_at,raid_actions,raid_dodges,raid_accepted,raid_rejected,raid_dodge_success,raid_active,raid_updates;
    int64_t raid_turn,raid_action_until,dodge_start,dodge_end,raid_update_serial,raid_update_turn;int32_t raid_status,raid_serial,raid_dodge_serial;unsigned raid_own,raid_pending_type,raid_bound,raid_moves_ready,raid_gate,raid_event_debug;int32_t raid_boss_moves[4];
    EnhMove raid_moves[1024];unsigned raid_move_count;
    uint64_t score_actor,score_boss;int32_t score_moves[3],score_types[3];double score_power[3],score_effect[3];unsigned score_ready;
    SilphManagedRef manual_raid_gui,manual_raid_root,item_menu;uint64_t manual_raid_until,manual_raid_next,item_menu_at;int64_t manual_raid_seed;unsigned manual_raid_gate,item_menu_done;char manual_raid_gym[161];
    SilphManagedRef throw_input,throw_input_owner;void *throw_input_stack;uint64_t throw_input_id,throw_input_serial,throw_input_at;unsigned throw_input_collect;
    CollectionView collection;
    void *capture_scope,*release_class;uint64_t capture_token,capture_encounter,capture_sent,transfer_next;uint32_t capture_source;
    EnhTransfer transfers[64];unsigned transfer_head,transfer_count;SilphManagedRef transfer_pending;
    uint64_t care_team[18],care_target,care_at,care_next,care_sync_until,raid_terminal_at;unsigned care_count,care_index,care_ready;int32_t care_hp,care_expected;
    SilphManagedRef care_pending;int64_t raid_charge_turn,raid_dodge_for,raid_pending_charge_turn,raid_dodge_seen_turn;int32_t raid_charge_move;uint64_t raid_event_types,raid_clock_at;int64_t raid_clock_turn,raid_last_dodge_turn;unsigned raid_primary_events,raid_clock_debug;
#ifdef SILPH_PILOT_AUTORAID
    AutoRaid route;
#endif
} Enhancer;
static _Atomic(Enhancer *) current;
static void (*enter_original)(void *,const void *),(*intro_original)(void *,const void *),(*exit_original)(void *,const void *);
static void *(*attempt_original)(void *,EnhThrow,const void *);
static void *(*push_input_original)(void *,void *,const void *);
static void (*launch_original)(void *,EnhPoint,const void *);
static void (*ground_original)(void *,void *,void *,const void *);
static void (*nameplate_original)(void *,void *,int32_t,const void *);
static void (*raid_enter_original)(void *,void *,void *,const void *),(*raid_exit_original)(void *,const void *);
static void (*raid_update_original[4])(void *,void *,const void *);
static void (*raid_turn_original)(void *,int64_t,const void *);
static void *(*raid_submit_original)(void *,void *,bool,const void *);
static uint64_t clock_ns(void){struct timespec t={0};clock_gettime(CLOCK_MONOTONIC,&t);return(uint64_t)t.tv_sec*UINT64_C(1000000000)+t.tv_nsec;}
typedef struct {Enhancer *s;unsigned depth;} EnhScope;
static EnhScope scope(Enhancer *s){EnhScope v={s,s?s->depth:0};if(s)s->depth++;return v;}
static void leave(EnhScope *v){if(v->s)v->s->depth=v->depth;}
#define ENH_SCOPE(s) EnhScope guard __attribute__((unused,cleanup(leave)))=scope(s)
static Enhancer *owned(void){Enhancer *s=atomic_load_explicit(&current,memory_order_acquire);return s&&!s->closed&&s->owner==(unsigned)syscall(__NR_gettid)?s:NULL;}
static bool allowed(Enhancer *s){return s&&s->allowed(s->allowed_context);}
static void drop(Enhancer *s,SilphManagedRef *ref){if(ref->serial){silph_managed_release(s->managed,*ref);*ref=(SilphManagedRef){0};}}
static bool target(Enhancer *s,SilphManagedRef ref,void **out){*out=NULL;return ref.serial&&!silph_managed_target(s->managed,ref,out)&&*out;}
static bool exact(Enhancer *s,void *o,void *k){return o&&k&&silph_callback_object(o,k)&&s->base.object_get_class(o)==k;}
static bool type(Enhancer *s,const void *t,const char *expected){const char *n=t?s->api.type_name(t):NULL;bool ok=n&&!strcmp(n,expected);if(n)s->api.free((void *)n);return ok;}
static void *klass(Enhancer *s,const char *assembly,const char *space,const char *name){size_t n=0;const void **all=s->api.assemblies(s->base.domain_get(),&n);if(!all||n>2048)return NULL;for(size_t i=0;i<n;i++){const void *im=s->api.image(all[i]);const char *actual=im?s->api.image_name(im):NULL;if(actual&&!strcmp(actual,assembly))return s->api.klass(im,space,name);}return NULL;}
static const void *field_uncached(Enhancer *s,void *k,const char *name,const char *expected){for(unsigned i=0;k&&i<12;i++,k=s->api.parent(k)){const void *f=s->api.field(k,name);if(f)return !(s->api.field_flags(f)&0x50u)&&type(s,s->api.field_type(f),expected)?f:NULL;}return NULL;}
static const void *enum_field(Enhancer *s,void *k,const char *name){const void *f=k?s->api.field(k,name):NULL;if(!f||(s->api.field_flags(f)&0x50u))return NULL;void *e=s->api.class_from_type(s->api.field_type(f));return e&&s->api.is_enum(e)&&s->base.type_get_type(s->api.enum_base(e))==8?f:NULL;}
static const void *method_uncached(Enhancer *s,void *k,const char *name,const char *result,unsigned count,const char **args){const void *m=k?s->api.method(k,name,(int)count):NULL;uint32_t impl=0;if(!m||s->base.method_get_param_count(m)!=count||(s->base.method_get_flags(m,&impl)&0x10u)||s->base.method_is_generic(m)||(result&&!type(s,s->base.method_get_return_type(m),result)))return NULL;for(unsigned i=0;i<count;i++)if(!type(s,s->api.parameter(m,i),args[i]))return NULL;return m;}
#ifdef SILPH_PILOT_AUTORAID
static uint64_t raid_lookup_hash(uint64_t key,const char *text){if(text)for(;*text;text++){key^=(unsigned char)*text;key*=UINT64_C(1099511628211);}return key;}
static RaidLookup *raid_lookup(Enhancer *s,void *k,const char *name,const char *result,unsigned count,const char **args,unsigned kind){
    if(!(s->config.flags&SILPH_ENH_AUTORAID))return NULL;
    uint64_t key=raid_lookup_hash(UINT64_C(1469598103934665603),name);key=raid_lookup_hash(key,result);key^=((uint64_t)count<<32)|kind;
    for(unsigned i=0;i<count;i++){key^=255;key=raid_lookup_hash(key,args[i]);}
    RaidLookup *entry=&s->route.lookup[(key^((uintptr_t)k>>4))&255];if(entry->klass!=k||entry->key!=key)*entry=(RaidLookup){.klass=k,.key=key};return entry;
}
#endif
static const void *field(Enhancer *s,void *k,const char *name,const char *expected){
#ifdef SILPH_PILOT_AUTORAID
    RaidLookup *entry=raid_lookup(s,k,name,expected,0,NULL,1);if(entry&&entry->value)return entry->value;
#endif
    const void *value=field_uncached(s,k,name,expected);
#ifdef SILPH_PILOT_AUTORAID
    if(entry)entry->value=value;
#endif
    return value;
}
static const void *method(Enhancer *s,void *k,const char *name,const char *result,unsigned count,const char **args){
#ifdef SILPH_PILOT_AUTORAID
    RaidLookup *entry=raid_lookup(s,k,name,result,count,args,2);if(entry&&entry->value)return entry->value;
#endif
    const void *value=method_uncached(s,k,name,result,count,args);
#ifdef SILPH_PILOT_AUTORAID
    if(entry)entry->value=value;
#endif
    return value;
}
static const void *unique_method(Enhancer *s,void *k,const char *result,unsigned count,const char **args){void *it=NULL;const void *found=NULL;for(unsigned i=0;k&&i<1024;i++){const void *m=s->base.class_get_methods(k,&it);if(!m)break;const char *name=s->base.method_get_name(m);if(name&&method(s,k,name,result,count,args)==m){if(found)return NULL;found=m;}}return found;}
static bool invoke(Enhancer *s,const void *m,void *o,void **args,void **out){if(!m||!o)return false;void *receiver=o;void *k=s->base.object_get_class(o);if(!k)return false;if(s->api.is_value(k)){receiver=s->base.object_unbox(o);if(!receiver)return false;}void *exception=NULL,*v=s->base.runtime_invoke(m,receiver,args,&exception);if(out)*out=v;return !exception;}
static bool scalar(Enhancer *s,const void *m,void *o,void *out,size_t n){void *boxed=NULL;if(!invoke(s,m,o,NULL,&boxed)||!boxed)return false;void *p=s->base.object_unbox(boxed);if(!p)return false;memcpy(out,p,n);return true;}
static bool keep(Enhancer *s,void *object,SilphManagedRef *ref,bool weak){if(!object)return false;SilphManagedRef next={0};if(silph_managed_retain(s->managed,object,weak,&next))return false;drop(s,ref);*ref=next;return true;}
static int hook(Enhancer *s,const void *m,void *proxy,void **original){if(!m||s->used>=sizeof(s->hooks)/sizeof(s->hooks[0]))return 1;uintptr_t address=0;memcpy(&address,(const char *)m+s->offset,sizeof(address));if((address&3)||!s->address(address,s->context))return 1;EnhHook *h=&s->hooks[s->used];h->address=address;memcpy(h->before,(void *)address,16);h->stub=shadowhook_hook_func_addr((void *)address,proxy,original);if(!h->stub)return 1;s->used++;return 0;}
static bool unity_alive(Enhancer *,void *);
#include "enhancer-display.inc"
static bool promise_value(Enhancer *,SilphManagedRef *,void *,void **,bool *);
static void *attempt(void *,EnhThrow,const void *);
#include "enhancer-transfer.inc"
#include "enhancer-care.inc"
#include "enhancer-raid.inc"
#ifdef SILPH_PILOT_AUTORAID
#include "autoraid-flow.inc"
#include "enhancer-raid-results.inc"
#endif
#include "enhancer-item-input.inc"
#include "collection-view.inc"
#include "nearby-tools.inc"
static bool unity_alive(Enhancer *s,void *o){if(!o)return false;if(!s->unity_cached)return true;uintptr_t value=0;s->api.get(o,s->unity_cached,&value);return value!=0;}
static void remember(Enhancer *s,void *interaction){
    if(!exact(s,interaction,s->interaction_class))return;void *state=NULL,*map=NULL;uint64_t id=0;
    if(!invoke(s,s->state_get,interaction,NULL,&state)||!exact(s,state,s->state_class)||!invoke(s,s->state_map,state,NULL,&map)||!map||!s->api.assignable(s->map_class,s->base.object_get_class(map))||!scalar(s,method(s,s->base.object_get_class(map),"get_EncounterId","System.UInt64",0,NULL),map,&id,8)||!id)return;
    if(id!=s->last_id){s->last_id=id;s->berry_tried=0;s->metrics.encounters++;s->metrics.encounter=id;bool shiny=false;if(s->state_shiny)scalar(s,s->state_shiny,state,&shiny,1);s->shiny=shiny;}
    if(keep(s,interaction,&s->interaction,true)&&keep(s,state,&s->state,true))s->berry_at=clock_ns()+UINT64_C(100000000);
}
static void tune_camera(Enhancer *s,void *o){
    if(!(s->config.flags&SILPH_ENH_INTRO)||!s->camera||!exact(s,o,s->interaction_class))return;void *camera=NULL;s->api.get(o,s->camera,&camera);if(!exact(s,camera,s->camera_class))return;
    EnhTiming *t=NULL;for(unsigned i=0;i<s->timing_count;i++){void *previous=NULL;if(target(s,s->timings[i].object,&previous)&&unity_alive(s,previous)){if(previous==camera)return;}else if(!t)t=&s->timings[i];}
    if(!t){if(s->timing_count>=12)return;t=&s->timings[s->timing_count++];}drop(s,&t->object);if(!keep(s,camera,&t->object,true))return;
    s->api.get(camera,s->camera_tween,&t->tween);s->api.get(camera,s->camera_intro,&t->intro);float quick=.01f;s->api.set(camera,s->camera_tween,&quick);s->api.set(camera,s->camera_intro,&quick);
}
static void enter(void *o,const void *m){Enhancer *s=owned();ENH_SCOPE(s);enter_original(o,m);if(allowed(s)){remember(s,o);tune_camera(s,o);}}
static void intro(void *o,const void *m){Enhancer *s=owned();ENH_SCOPE(s);intro_original(o,m);if(allowed(s))remember(s,o);}
static void exit_state(void *o,const void *m){Enhancer *s=owned();ENH_SCOPE(s);void *old=NULL;if(s&&target(s,s->interaction,&old)&&old==o){manual_input_release(s);drop(s,&s->interaction);drop(s,&s->state);}exit_original(o,m);}
static void *attempt(void *o,EnhThrow data,const void *m){
    Enhancer *s=owned();ENH_SCOPE(s);bool active=allowed(s)&&exact(s,o,s->interaction_class);if(active){remember(s,o);s->throw_serial++;if((s->metrics.available&SILPH_ENH_AIM)&&data.ball>=1&&data.ball<=8&&data.ball!=4){data.missed=0;data.bullseye=1;}if((s->config.flags&SILPH_ENH_THROW)&&!data.missed){const float sizes[]={0,1.1f,.85f,.5f,.05f};data.reticle=sizes[s->config.quality];data.bullseye=1;if(s->config.flags&SILPH_CFG_CURVE)data.spinning=1;}s->metrics.throws++;}
    if(active&&!s->promise.serial){uint32_t source=SILPH_SOURCE_UNKNOWN;void *state=NULL,*map=NULL;if(target(s,s->state,&state)&&invoke(s,s->state_map,state,NULL,&map)&&map){int32_t kind=0;if(scalar(s,method(s,s->base.object_get_class(map),"get_EncounterType","Holoholo.Rpc.EncounterType",0,NULL),map,&kind,4))source=encounter_source(kind);}capture_begin(s,s->last_id,source,clock_ns());__android_log_print(ANDROID_LOG_INFO,"SilphEnhancer","manual throw encounter=%llu ball=%d pending=0",(unsigned long long)s->last_id,data.ball);}
    if(active&&!s->promise.serial)manual_input_begin(s,o);void *promise=attempt_original(o,data,m);if(active){s->throw_input_collect=0;s->throw_input_stack=NULL;}if(active&&promise&&!s->promise.serial){if(keep(s,promise,&s->promise,false)){s->promise_at=clock_ns();s->promise_id=s->last_id;s->promise_shiny=s->shiny;void *state=NULL;if(target(s,s->state,&state))keep(s,state,&s->promise_state,true);}else capture_abandon(s);}if(active&&!promise){capture_abandon(s);manual_input_release(s);}return promise;
}
static void launch(void *o,EnhPoint flick,const void *m){
    Enhancer *s=owned();ENH_SCOPE(s);bool steer=false;void *body=NULL;EnhVector velocity={0};
    if(allowed(s)&&(s->config.flags&SILPH_ENH_AIM)&&exact(s,o,s->ball_class)){
        void *interaction=NULL,*active=NULL,*transform=NULL,*reticle=NULL,*target_transform=NULL;int32_t item=0;
        if(target(s,s->interaction,&interaction)&&invoke(s,s->interaction_ball,interaction,NULL,&active)&&active==o&&scalar(s,s->ball_type,o,&item,4)&&item>=1&&item<=8&&item!=4){
            s->api.get(o,s->ball_reticle,&reticle);s->api.get(o,s->ball_body,&body);
            const void *reticle_transform=reticle?method(s,s->base.object_get_class(reticle),"get_Transform","UnityEngine.Transform",0,NULL):NULL;
            EnhVector from={0},to={0};
            if(body&&invoke(s,s->ball_transform,o,NULL,&transform)&&transform&&invoke(s,reticle_transform,reticle,NULL,&target_transform)&&target_transform&&scalar(s,s->transform_position,transform,&from,12)&&scalar(s,s->transform_position,target_transform,&to,12)){
                float dx=to.x-from.x,dy=to.y-from.y,dz=to.z-from.z,d2=dx*dx+dy*dy+dz*dz;
                if(isfinite(d2)&&d2>.0001f&&d2<10000){EnhVector near={from.x+dx*.92f,from.y+dy*.92f,from.z+dz*.92f};velocity=(EnhVector){(to.x-near.x)*25,(to.y-near.y)*25,(to.z-near.z)*25};void *args[]={&near};steer=invoke(s,s->set_position,transform,args,NULL);if(steer)flick=(EnhPoint){0,1};}
            }
        }
    }
    launch_original(o,flick,m);if(steer){void *args[]={&velocity};invoke(s,s->set_velocity,body,args,NULL);}
}
static void ground(void *o,void *sender,void *event,const void *m){
    Enhancer *s=owned();ENH_SCOPE(s);ground_original(o,sender,event,m);if(!allowed(s)||!s->map||!(s->config.flags&SILPH_ENH_TELEPORT)||!exact(s,o,s->ground_class))return;
    void *gesture=NULL,*controller=NULL;s->api.get(o,s->ground_gesture,&gesture);s->api.get(o,s->ground_controller,&controller);if(!exact(s,gesture,s->gesture_class)||!controller)return;
    EnhPoint point={0};s->api.get(gesture,s->gesture_position,&point);uint64_t now=clock_ns();float dx=point.x-s->last_tap.x,dy=point.y-s->last_tap.y;
    bool twice=s->tap_at&&now>s->tap_at&&now-s->tap_at>=UINT64_C(50000000)&&now-s->tap_at<=UINT64_C(350000000)&&dx*dx+dy*dy<=6400;
    s->tap_at=now;s->last_tap=point;if(!twice)return;s->tap_at=0;
    const char *args[]={"UnityEngine.Vector2","Niantic.LatLng&"};const void *convert=method(s,s->base.object_get_class(controller),"GetGroundLocation","System.Boolean",2,args);EnhLocation location={0};void *in[]={&point,&location},*boxed=NULL;bool ok=false;
    if(!invoke(s,convert,controller,in,&boxed)||!boxed)return;void *raw=s->base.object_unbox(boxed);if(!raw)return;memcpy(&ok,raw,1);
    if(ok&&isfinite(location.latitude)&&isfinite(location.longitude)&&fabs(location.latitude)<=90&&fabs(location.longitude)<=180){s->metrics.latitude=location.latitude;s->metrics.longitude=location.longitude;s->metrics.jumps++;__android_log_print(ANDROID_LOG_INFO,"SilphEnhancer","map gesture accepted request=%llu",(unsigned long long)s->metrics.jumps);}
}
static bool promise_value(Enhancer *s,SilphManagedRef *ref,void *expected,void **out,bool *pending){
    *pending=false;*out=NULL;void *promise=NULL;if(!target(s,*ref,&promise))return false;void *k=s->base.object_get_class(promise);const void *done=field(s,k,"completeCalled","System.Boolean"),*failed=field(s,k,"errorCalled","System.Boolean");
    if(!done||!failed)return false;bool complete=false,error=false;s->api.get(promise,done,&complete);s->api.get(promise,failed,&error);if(!complete&&!error){*pending=true;return true;}if(error)return false;
    const char *name=s->api.class_name(expected);char full[128];if(!name)return false;int n=snprintf(full,sizeof(full),"Holoholo.Rpc.%s",name);if(n<0||(size_t)n>=sizeof(full))return false;
    const void *value=field(s,k,"completedValue",full);if(!value)return false;s->api.get(promise,value,out);return exact(s,*out,expected);
}
static bool fast_breakout(Enhancer *s,void *reply,uint64_t now){
    if(!(s->config.flags&SILPH_ENH_FAST)||s->last_id!=s->promise_id)return false;
#ifdef SILPH_PILOT_AUTORAID
    if(s->config.flags&SILPH_ENH_AUTORAID)return false;
#endif
    void *interaction=NULL,*state=NULL,*pending=NULL,*pokemon=NULL,*ball=NULL;bool active=false;
    if(!target(s,s->interaction,&interaction)||!target(s,s->state,&state)||!target(s,s->promise_state,&pending)||state!=pending||!scalar(s,method(s,s->interaction_class,"get_IsActiveState","System.Boolean",0,NULL),interaction,&active,1)||!active)return false;
    int32_t kind=0,remaining=-1;scalar(s,method(s,s->state_class,"get_EncounterType","Holoholo.Rpc.EncounterType",0,NULL),state,&kind,4);
    if((kind==3||s->raid_display_encounter==s->promise_id)&&(!read_i32(s,reply,"ThrowsRemaining",&remaining,false)||remaining<=0))return false;
    const void *broke=method(s,s->interaction_class,"PokemonBrokeOut","System.Void",0,NULL);
    if(!broke||!read_ref(s,interaction,"get_EncounterPokemon",&pokemon)||!read_ref(s,interaction,"get_Pokeball",&ball))return false;
    const void *escape=method(s,s->base.object_get_class(pokemon),"BreakoutFromPokeball","System.Void",0,NULL);const char *boolean[]={"System.Boolean"};
    const void *invulnerable=method(s,s->base.object_get_class(pokemon),"set_Invulnerable","System.Void",1,boolean),*activate=method(s,s->base.object_get_class(ball),"SetActive","System.Void",1,boolean);
    if(!escape||!invulnerable||!activate)return false;
    bool no=false,yes=true;void *off[]={&no},*on[]={&yes};int32_t empty=0;void *berry[]={&empty};
    if(!invoke(s,broke,interaction,NULL,NULL)||!invoke(s,escape,pokemon,NULL,NULL)||!invoke(s,invulnerable,pokemon,off,NULL)||!invoke(s,activate,ball,on,NULL))return false;
    invoke(s,s->set_berry,interaction,berry,NULL);
    void *response=NULL,*probability=NULL;if(read_ref(s,state,"get_EncounterResponse",&response)&&object_field(s,response,"BaseCaptureProbability","Holoholo.Rpc.CaptureProbabilityProto",&probability)){
        const char *types[]={"Holoholo.Rpc.CaptureProbabilityProto"};void *args[]={probability};invoke(s,method(s,s->interaction_class,"set_CaptureProbability","System.Void",1,types),interaction,args,NULL);
    }
    s->berry_at=now+UINT64_C(50000000);if(s->throw_input.serial)s->throw_input_at=now;
    __android_log_print(ANDROID_LOG_INFO,"SilphEnhancer","capture breakout restored encounter=%llu",(unsigned long long)s->promise_id);return true;
}
static void poll_capture(Enhancer *s,void *read,void *managed,uint64_t now){
    if(!s->promise.serial)return;void *result=NULL;bool pending=false;bool ok=promise_value(s,&s->promise,s->catch_class,&result,&pending);if(pending){if(now-s->promise_at<UINT64_C(30000000000))return;ok=false;}
    if(ok&&result){int32_t status=0;const void *f=enum_field(s,s->catch_class,"Status");
        if(!f){s->metrics.error=1;capture_abandon(s);drop(s,&s->promise);drop(s,&s->promise_state);return;}s->api.get(result,f,&status);
#ifdef SILPH_PILOT_AUTORAID
        if(!ar_headless_response(s,result,status)){capture_abandon(s);drop(s,&s->promise);drop(s,&s->promise_state);return;}
#endif
        if(status==1||status==3){if(status==1){s->metrics.catches++;s->metrics.shinies+=s->promise_shiny;SilphManagedRef response={0};uint64_t xp=0,dust=0;int reward=silph_managed_retain(managed,result,false,&response);if(!reward)reward=silph_capture_rewards(read,managed,response,&xp,&dust);if(response.serial)silph_managed_release(managed,response);if(reward)s->metrics.reward_error=(uint32_t)reward;else{s->metrics.xp+=xp;s->metrics.stardust+=dust;}const void *captured=field(s,s->catch_class,"CapturedPokemonId","System.UInt64");if(captured)s->api.get(result,captured,&s->metrics.captured);}else s->metrics.fled++;
            capture_settle(s,s->promise_id,s->metrics.captured,status==1,now);
            if(s->config.flags&SILPH_ENH_FAST){void *state=NULL;void *pending_state=NULL;if(s->last_id==s->promise_id&&target(s,s->state,&state)&&target(s,s->promise_state,&pending_state)&&state==pending_state){int32_t reason=status==1?0:2;void *args[]={&reason};if(!invoke(s,s->state_complete,state,args,NULL))s->metrics.error=2;}}
        }else if((status==2||status==4)&&s->last_id==s->promise_id){s->berry_tried=0;s->berry_at=now+UINT64_C(500000000);if(s->throw_input.serial)s->throw_input_at=now+UINT64_C(500000000);fast_breakout(s,result,now);}
        if(status>=1&&status<=4)s->metrics.capture_notice=((++s->notice_sequence&8191u)<<3)|(uint32_t)status;
        if(status==1||status==3||status<1||status>4)manual_input_release(s);
        #ifdef SILPH_PILOT_AUTORAID
        if((s->config.flags&SILPH_ENH_AUTORAID)&&s->route.headless&&(status==2||status==4)){s->berry_at=now+UINT64_C(50000000);s->route.ui_at=s->berry_at;}
        #endif
        int32_t remaining=-1;read_i32(s,result,"ThrowsRemaining",&remaining,false);bool in_play=false;void *interaction=NULL;if(target(s,s->interaction,&interaction))scalar(s,s->ball_in_play,interaction,&in_play,1);
        __android_log_print(ANDROID_LOG_INFO,"SilphEnhancer","manual capture result=%d catches=%llu throws=%llu remaining=%d inPlay=%u elapsedMs=%llu",status,(unsigned long long)s->metrics.catches,(unsigned long long)s->metrics.throws,remaining,in_play,(unsigned long long)((now-s->promise_at)/1000000));
    }else{s->metrics.error=3;manual_input_release(s);__android_log_print(ANDROID_LOG_INFO,"SilphEnhancer","manual capture response unconfirmed; original flow retained");}capture_abandon(s);drop(s,&s->promise);drop(s,&s->promise_state);
}
static void berries(Enhancer *s,void *game,void *managed,uint64_t now){
    void *interaction=NULL;target(s,s->interaction,&interaction);
    if(s->berry_promise.serial){void *value=NULL;bool pending=false;bool ok=promise_value(s,&s->berry_promise,s->berry_class,&value,&pending);if(pending){if(now-s->berry_at<UINT64_C(15000000000))return;ok=false;}
        int32_t status=-1;if(ok&&value){const void *f=enum_field(s,s->berry_class,"Status");if(f)s->api.get(value,f,&status);}
        if(ok&&status==0){void *owner=NULL;bool in_play=true;if(interaction&&s->last_id==s->berry_id&&s->throw_serial==s->berry_throw_serial&&target(s,s->berry_interaction,&owner)&&owner==interaction&&scalar(s,s->ball_in_play,interaction,&in_play,1)&&!in_play){int32_t berry=(int32_t)s->pending_berry;void *args[]={&berry};invoke(s,s->set_berry,interaction,args,NULL);}s->metrics.berries++;}
#ifdef SILPH_PILOT_AUTORAID
        if((s->config.flags&SILPH_ENH_AUTORAID)&&s->route.headless)ar_hidden_berry_result(s,status,ok,now);
#endif
        __android_log_print(ANDROID_LOG_INFO,"SilphEnhancer","manual berry item=%u result=%d confirmed=%llu",s->pending_berry,status,(unsigned long long)s->metrics.berries);drop(s,&s->berry_promise);drop(s,&s->berry_interaction);return;
    }
    if(!interaction||!(s->metrics.available&SILPH_CFG_BERRIES)||s->berry_tried||s->promise.serial||s->throw_input.serial||now<s->berry_at)return;bool can=false,in_play=true;int32_t active=0;
    if(!scalar(s,s->ball_in_play,interaction,&in_play,1)||in_play||!scalar(s,s->can_berry,interaction,&can,1)||!can||!scalar(s,s->active_berry,interaction,&active,4)||active)return;
    s->berry_tried=1;uint32_t selected=0;for(unsigned i=0;i<s->config.berry_count;i++){uint32_t count=0;if(!silph_game_read_item(game,(int32_t)s->config.berries[i],&count)&&count){selected=s->config.berries[i];break;}}if(!selected){
#ifdef SILPH_PILOT_AUTORAID
        if(s->config.flags&SILPH_ENH_AUTORAID)__android_log_print(ANDROID_LOG_INFO,"SilphRaid","selected berries unavailable; capture continues");
#endif
        return;}
    SilphManagedRef bag_ref={0};if(silph_game_read_item_bag(game,&bag_ref))return;void *bag=NULL,*state=NULL,*map=NULL,*promise=NULL;
    if(!silph_managed_target(managed,bag_ref,&bag)&&target(s,s->state,&state)&&invoke(s,s->state_map,state,NULL,&map)&&map){const char *types[]={"Holoholo.Rpc.Item","Niantic.Holoholo.Map.IItemEncounterContext"};const void *use=method(s,s->base.object_get_class(bag),"UseItemForEncounter",NULL,2,types);int32_t item=(int32_t)selected;void *args[]={&item,map};
        if(invoke(s,use,bag,args,&promise)&&promise&&keep(s,promise,&s->berry_promise,false)){s->pending_berry=selected;s->berry_at=now;s->berry_id=s->last_id;s->berry_throw_serial=s->throw_serial;keep(s,interaction,&s->berry_interaction,true);}
#ifdef SILPH_PILOT_AUTORAID
        else if(s->config.flags&SILPH_ENH_AUTORAID)__android_log_print(ANDROID_LOG_INFO,"SilphRaid","berry request unavailable item=%u method=%u",selected,use!=NULL);
#endif
    }
    silph_managed_release(managed,bag_ref);
}
size_t silph_enhancer_size(void){return sizeof(Enhancer);}
bool silph_enhancer_busy(void *memory){Enhancer *s=memory;return s&&(s->depth||s->promise.serial||s->berry_promise.serial||s->interaction.serial||s->raid.serial||s->transfer_pending.serial||s->care_pending.serial);}
int silph_enhancer_open(void *memory,size_t size,const SilphManagedApi *base,SilphManagedResolver resolver,void *context,uint32_t offset,const SilphSessionConfig *config,SilphVisualAllowed allow,void *allow_context,SilphVisualAddress address){
    if(!memory||size!=sizeof(Enhancer)||!base||!resolver||!config||config->mode!=5||!allow||!address||current||offset>120||offset%8)return 1;Enhancer *s=memory;memset(s,0,size);s->base=*base;s->config=*config;s->context=context;s->offset=offset;s->allowed=allow;s->allowed_context=allow_context;s->address=address;s->owner=(unsigned)syscall(__NR_gettid);
    if(!(config->flags&SILPH_ENH_AUTORAID))__android_log_print(ANDROID_LOG_INFO,"SilphEnhancer","startup features=%u collection=%u buddy=%u owner=%u",config->flags,config->collection_info,config->buddy_flags,s->owner);
#define LOAD(member,name) do{void *p=resolver("il2cpp_" name,context);if(!p)return 1;memcpy(&s->api.member,&p,sizeof(p));}while(0)
    LOAD(assemblies,"domain_get_assemblies");LOAD(image,"assembly_get_image");LOAD(image_name,"image_get_name");LOAD(klass,"class_from_name");LOAD(parent,"class_get_parent");LOAD(nested,"class_get_nested_types");LOAD(class_name,"class_get_name");LOAD(assignable,"class_is_assignable_from");LOAD(method,"class_get_method_from_name");LOAD(parameter,"method_get_param");LOAD(field,"class_get_field_from_name");LOAD(fields,"class_get_fields");LOAD(field_type,"field_get_type");LOAD(field_flags,"field_get_flags");LOAD(type_name,"type_get_name");LOAD(free,"free");LOAD(get,"field_get_value");LOAD(set,"field_set_value");LOAD(set_object,"field_set_value_object");LOAD(object_new,"object_new");LOAD(type_object,"type_get_object");LOAD(value_box,"value_box");LOAD(chars,"string_chars");LOAD(value_size,"class_value_size");LOAD(class_from_type,"class_from_type");LOAD(is_enum,"class_is_enum");LOAD(is_value,"class_is_valuetype");LOAD(enum_base,"class_enum_basetype");
#undef LOAD
#ifdef SILPH_PILOT_AUTORAID
    if((config->flags&(SILPH_ENH_AUTORAID|SILPH_ENH_LIGHT_GRAPHICS))==(SILPH_ENH_AUTORAID|SILPH_ENH_LIGHT_GRAPHICS)){
        void *(*resolve)(const char *)=NULL;void *symbol=resolver("il2cpp_resolve_icall",context);if(symbol)memcpy(&resolve,&symbol,sizeof(resolve));
        const char *getters[]={"UnityEngine.QualitySettings::get_shadows()","UnityEngine.QualitySettings::get_antiAliasing()","UnityEngine.QualitySettings::get_pixelLightCount()","UnityEngine.Application::get_targetFrameRate()"};
        const char *setters[]={"UnityEngine.QualitySettings::set_shadows(UnityEngine.ShadowQuality)","UnityEngine.QualitySettings::set_antiAliasing(System.Int32)","UnityEngine.QualitySettings::set_pixelLightCount(System.Int32)","UnityEngine.Application::set_targetFrameRate(System.Int32)"};
        for(unsigned i=0;resolve&&i<4;i++){void *get=resolve(getters[i]),*set=resolve(setters[i]);if(get&&set){memcpy(&s->route.graphics_get[i],&get,sizeof(get));memcpy(&s->route.graphics_set[i],&set,sizeof(set));}}
    }
#endif
    uint32_t references=config->collection_info?384:96;s->managed=calloc(1,silph_managed_size(references));if(!s->managed)return 1;int opened=silph_managed_open(s->managed,silph_managed_size(references),base,references,64);if(opened){free(s->managed);s->managed=NULL;__android_log_print(ANDROID_LOG_ERROR,"SilphEnhancer","manual reference context unavailable error=%d",opened);return 1;}
    const char *space="Niantic.Holoholo.Encounter";s->interaction_class=klass(s,"holo-game.dll",space,"EncounterInteractionState");s->state_class=klass(s,"holo-game.dll",space,"EncounterState");s->map_class=klass(s,"holo-game.dll","Niantic.Holoholo.Map","MapPokemon");s->catch_class=klass(s,"holo-protos.dll","Holoholo.Rpc","CatchPokemonOutProto");s->berry_class=klass(s,"holo-protos.dll","Holoholo.Rpc","UseItemEncounterOutProto");
    s->state_get=unique_method(s,s->interaction_class,"Niantic.Holoholo.Encounter.IEncounterState",0,NULL);s->state_shiny=method(s,s->state_class,"get_IsEncounteredPokemonShiny","System.Boolean",0,NULL);s->state_map=method(s,s->state_class,"get_MapPokemon","Niantic.Holoholo.Map.MapPokemon",0,NULL);
    const char *reason[]={"Niantic.Holoholo.Encounter.EncounterResult"};s->state_complete=method(s,s->state_class,"EncounterStateComplete","System.Void",1,reason);s->map_id=method(s,s->map_class,"get_EncounterId","System.UInt64",0,NULL);
    s->active_berry=method(s,s->interaction_class,"get_ActiveBerry","Holoholo.Rpc.Item",0,NULL);s->can_berry=method(s,s->interaction_class,"get_CanUseBerry","System.Boolean",0,NULL);s->ball_in_play=method(s,s->interaction_class,"get_PokeballInPlay","System.Boolean",0,NULL);const char *item[]={"Holoholo.Rpc.Item"};s->set_berry=method(s,s->interaction_class,"set_ActiveBerry","System.Void",1,item);
    if(!s->interaction_class||!s->state_get||!s->state_map||!s->state_complete||!s->map_id||!s->catch_class){__android_log_print(ANDROID_LOG_ERROR,"SilphEnhancer","manual contract unavailable interaction=%u state=%u map=%u finish=%u id=%u response=%u",s->interaction_class!=NULL,s->state_get!=NULL,s->state_map!=NULL,s->state_complete!=NULL,s->map_id!=NULL,s->catch_class!=NULL);return 2;}
    void *throw_class=klass(s,"holo-game.dll",space,"PokeballThrow");uint32_t align=0;const char *throw_args[]={"Niantic.Holoholo.Encounter.PokeballThrow"};const void *throw_method=method(s,s->interaction_class,"AttemptCapture",NULL,1,throw_args);
    if(!throw_class||s->api.value_size(throw_class,&align)!=12||!throw_method)return 3;
    atomic_store_explicit(&current,s,memory_order_release);
    if(hook(s,method(s,s->interaction_class,"EnterState","System.Void",0,NULL),(void *)enter,(void **)&enter_original)||hook(s,method(s,s->interaction_class,"IntroCompleted","System.Void",0,NULL),(void *)intro,(void **)&intro_original)||hook(s,method(s,s->interaction_class,"ExitState","System.Void",0,NULL),(void *)exit_state,(void **)&exit_original)||hook(s,throw_method,(void *)attempt,(void **)&attempt_original))return 4;
    s->metrics.available=s->config.flags&(SILPH_ENH_FAST|SILPH_ENH_THROW);
    if(s->berry_class&&s->active_berry&&s->can_berry&&s->ball_in_play&&s->set_berry&&enum_field(s,s->berry_class,"Status"))s->metrics.available|=s->config.flags&SILPH_CFG_BERRIES;
    s->unity_cached=field(s,klass(s,"UnityEngine.CoreModule.dll","UnityEngine","Object"),"m_CachedPtr","System.IntPtr");
    s->camera_class=klass(s,"holo-game.dll",space,"EncounterParkCameraController");s->camera=field(s,s->interaction_class,"parkCameraController","Niantic.Holoholo.Encounter.EncounterParkCameraController");s->camera_tween=field(s,s->camera_class,"tweenDurationSeconds","System.Single");s->camera_intro=field(s,s->camera_class,"introDurationSeconds","System.Single");
    if(s->camera&&s->camera_tween&&s->camera_intro)s->metrics.available|=s->config.flags&SILPH_ENH_INTRO;else s->camera=NULL;
    if(s->config.flags&SILPH_ENH_TELEPORT){s->ground_class=klass(s,"holo-game.dll","Niantic.Holoholo.Map","GroundTapHandler");s->gesture_class=klass(s,"TouchScript.dll","TouchScript.Gestures","TapGesture");s->ground_gesture=field(s,s->ground_class,"tapGesture","TouchScript.Gestures.TapGesture");s->ground_controller=field(s,s->ground_class,"emuq","Niantic.Holoholo.Map.IMapController");s->gesture_position=field(s,s->gesture_class,"startPosition","UnityEngine.Vector2");const char *args[]={"System.Object","System.EventArgs"};const void *tap=unique_method(s,s->ground_class,"System.Void",2,args);
        if(s->ground_gesture&&s->ground_controller&&s->gesture_position&&tap&&!hook(s,tap,(void *)ground,(void **)&ground_original))s->metrics.available|=SILPH_ENH_TELEPORT;}
    if(s->config.flags&SILPH_ENH_AIM){
        s->ball_class=klass(s,"holo-game.dll",space,"Pokeball");s->transform_class=klass(s,"UnityEngine.CoreModule.dll","UnityEngine","Transform");void *body=klass(s,"UnityEngine.PhysicsModule.dll","UnityEngine","Rigidbody");
        s->ball_type=method(s,s->ball_class,"get_BallType","Holoholo.Rpc.Item",0,NULL);s->ball_transform=method(s,s->ball_class,"get_Transform","UnityEngine.Transform",0,NULL);s->interaction_ball=method(s,s->interaction_class,"get_Pokeball","Niantic.Holoholo.Encounter.IPokeball",0,NULL);
        s->ball_reticle=field(s,s->ball_class,"dspd","Niantic.Holoholo.Encounter.IReticle");s->ball_body=field(s,s->ball_class,"body","UnityEngine.Rigidbody");s->transform_position=method(s,s->transform_class,"get_position","UnityEngine.Vector3",0,NULL);const char *vector[]={"UnityEngine.Vector3"};s->set_position=method(s,s->transform_class,"set_position","System.Void",1,vector);s->set_velocity=method(s,body,"set_linearVelocity","System.Void",1,vector);
        const char *point[]={"UnityEngine.Vector2"};const void *entry=method(s,s->ball_class,"Launch","System.Void",1,point);
        if(s->ball_type&&s->ball_transform&&s->ball_reticle&&s->ball_body&&s->interaction_ball&&s->transform_position&&s->set_position&&s->set_velocity&&entry&&!hook(s,entry,(void *)launch,(void **)&launch_original))s->metrics.available|=SILPH_ENH_AIM;
    }
    display_open(s);raid_open(s);manual_input_open(s);s->pokemon_class=klass(s,"holo-protos.dll","Holoholo.Rpc","PokemonProto");
    collection_open(s,resolver,context);
    nearby_open(s);
    if(config->flags&SILPH_ENH_TRANSFER){s->release_class=klass(s,"holo-protos.dll","Holoholo.Rpc","ReleasePokemonOutProto");uint32_t bytes=silph_core_size(50000);s->capture_scope=s->release_class?calloc(1,bytes):NULL;if(!s->capture_scope||silph_core_open(s->capture_scope,bytes,50000,0))return 5;s->metrics.available|=SILPH_ENH_TRANSFER;}
    if(config->flags&SILPH_ENH_RAID_HEAL)s->metrics.available|=SILPH_ENH_RAID_HEAL;
    s->metrics.failed=(config->flags&(SILPH_ENH_FAST|SILPH_ENH_THROW|SILPH_ENH_INTRO|SILPH_ENH_TELEPORT|SILPH_ENH_AIM|SILPH_CFG_BERRIES|SILPH_ENH_INFO|SILPH_ENH_RAID))&~s->metrics.available;
    __android_log_print(ANDROID_LOG_INFO,"SilphEnhancer","manual tools prepared available=%u failed=%u hooks=%u",s->metrics.available,s->metrics.failed,s->used);return 0;
}
int silph_enhancer_tick(void *memory,void *read,void *game,void *managed,const SilphSchedulePermit *permit,bool map,bool battle,uint64_t now,SilphEnhancerMetrics *out){
    Enhancer *s=memory;if(!s||s->closed||!out||s->owner!=(unsigned)syscall(__NR_gettid)||!silph_schedule_permitted(permit,true))return 1;s->map=map&&!battle;s->nearby_game=game;s->nearby_scope=managed;
    bool first_tick=!s->first_tick_logged&&!(s->config.flags&SILPH_ENH_AUTORAID);s->first_tick_logged=1;
#define STARTUP_PHASE(name) do{if(first_tick)__android_log_print(ANDROID_LOG_INFO,"SilphEnhancer","startup first tick phase=%s",name);}while(0)
    STARTUP_PHASE("display");
    display_tick(s,now);
    STARTUP_PHASE("raid");
    raid_tick(s,permit,now);
#ifdef SILPH_PILOT_AUTORAID
    STARTUP_PHASE("results");
    manual_raid_results(s,game,managed,now);
#endif
    if(s->map){if(s->raid.serial&&s->raid_status>1){if(!s->raid_terminal_at)s->raid_terminal_at=now;if(now-s->raid_terminal_at>=UINT64_C(500000000))raid_clear(s);}if(!s->promise.serial&&!s->berry_promise.serial){drop(s,&s->interaction);drop(s,&s->state);}}
    STARTUP_PHASE("capture");poll_capture(s,read,managed,now);
    STARTUP_PHASE("input");
    manual_input_tick(s,now);
    manual_item_input(s,now);
    bool safe=s->map&&!battle&&!s->raid.serial&&!s->interaction.serial&&!s->promise.serial&&!s->berry_promise.serial;
#ifdef SILPH_PILOT_AUTORAID
    if((s->config.flags&SILPH_ENH_AUTORAID)&&s->route.headless)safe=false;
#endif
    bool berry_allowed=true;
#ifdef SILPH_PILOT_AUTORAID
    if((s->config.flags&SILPH_ENH_AUTORAID)&&(s->route.phase!=5||s->route.stage!=6||!s->route.expected||s->route.expected!=s->last_id))berry_allowed=false;
#endif
    STARTUP_PHASE("berries");
    if(berry_allowed&&!battle&&!s->raid.serial&&!s->care_pending.serial&&!s->transfer_pending.serial)berries(s,game,managed,now);
    STARTUP_PHASE("transfer");
    transfer_tick(s,read,game,managed,permit,now,safe&&!s->care_pending.serial);
    STARTUP_PHASE("care");
    care_tick(s,read,game,managed,permit,now,safe&&!s->transfer_pending.serial);
#ifdef SILPH_PILOT_AUTORAID
    if(s->config.flags&SILPH_ENH_AUTORAID){int route=autoraid_tick(s,permit,now);if(route)return route;}
#endif
    STARTUP_PHASE("ready");
#undef STARTUP_PHASE
    s->metrics.pending=s->promise.serial||s->berry_promise.serial||s->transfer_pending.serial||s->care_pending.serial;*out=s->metrics;return 0;
}
int silph_enhancer_close(void *memory,SilphManagedMetrics *metrics){
    Enhancer *s=memory;if(!s||!metrics||s->owner!=(unsigned)syscall(__NR_gettid)||s->depth)return 1;s->closed=1;int r=0;
    for(unsigned i=0;i<s->used;i++){EnhHook *h=&s->hooks[i];if(h->stub){int code=shadowhook_unhook(h->stub);if(code||memcmp((void *)h->address,h->before,16)){r=1;continue;}h->stub=NULL;}}if(r)return r;atomic_store(&current,NULL);
    drop(s,&s->nearby_gui);collection_close(s);display_close(s);raid_clear(s);capture_abandon(s);manual_input_release(s);drop(s,&s->item_menu);drop(s,&s->manual_raid_gui);drop(s,&s->manual_raid_root);drop(s,&s->transfer_pending);drop(s,&s->care_pending);if(s->capture_scope){silph_core_close(s->capture_scope);free(s->capture_scope);s->capture_scope=NULL;}
#ifdef SILPH_PILOT_AUTORAID
    int route_closed=autoraid_close(s);if(route_closed)return route_closed;
#endif
    for(unsigned i=0;i<s->timing_count;i++){void *o=NULL;if(target(s,s->timings[i].object,&o)&&exact(s,o,s->camera_class)&&unity_alive(s,o)){s->api.set(o,s->camera_tween,&s->timings[i].tween);s->api.set(o,s->camera_intro,&s->timings[i].intro);}drop(s,&s->timings[i].object);}
    drop(s,&s->promise);drop(s,&s->promise_state);drop(s,&s->berry_promise);drop(s,&s->berry_interaction);drop(s,&s->interaction);drop(s,&s->state);if(s->managed){r=silph_managed_close(s->managed,metrics);if(!r){free(s->managed);s->managed=NULL;}}return r;
}
