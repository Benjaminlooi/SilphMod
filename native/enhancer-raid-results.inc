static void manual_raid_release(Enhancer *s){
    drop(s,&s->manual_raid_gui);drop(s,&s->manual_raid_root);s->manual_raid_until=0;s->manual_raid_seed=0;s->manual_raid_gym[0]=0;
}
static bool manual_raid_service(Enhancer *s,void *game,void *managed,unsigned kind,SilphManagedRef *out){
    SilphManagedRef ref={0};void *object=NULL;int r=silph_game_read_raid_service(game,kind,&ref);
    if(!r)r=silph_managed_target(managed,ref,&object);bool ok=!r&&keep(s,object,out,false);
    if(ref.serial)silph_managed_release(managed,ref);return ok;
}
static void manual_raid_gate(Enhancer *s,unsigned gate){
    if(s->manual_raid_gate!=gate){s->manual_raid_gate=gate;__android_log_print(ANDROID_LOG_INFO,"SilphEnhancer","manual raid result gate=%u",gate);}
}
static void manual_raid_results(Enhancer *s,void *game,void *managed,uint64_t now){
    if((s->config.flags&SILPH_ENH_AUTORAID)||!(s->config.flags&SILPH_CFG_SCENES)||!s->manual_raid_until||now<s->manual_raid_next)return;
    s->manual_raid_next=now+UINT64_C(200000000);
    if(now>=s->manual_raid_until||s->interaction.serial){manual_raid_release(s);return;}
    if(!s->manual_raid_root.serial&&!manual_raid_service(s,game,managed,2,&s->manual_raid_root)){manual_raid_gate(s,1);return;}
    if(!s->manual_raid_gui.serial&&!manual_raid_service(s,game,managed,3,&s->manual_raid_gui)){manual_raid_gate(s,2);return;}
    void *root=NULL,*service=NULL,*gui=NULL,*fort=NULL;char gym[161]={0};
    if(!target(s,s->manual_raid_root,&root)||!read_ref(s,root,"get_FortId",&fort)||!label_string(s,fort,gym,sizeof(gym))||!gym[0]){manual_raid_gate(s,3);return;}
    void *raid=NULL;int64_t current=0;
    if(!read_ref(s,root,"get_RaidInfo",&raid)||!read_i64(s,raid,"RaidSeed",&current)||!current){manual_raid_gate(s,9);return;}
    if(!s->manual_raid_gym[0]){snprintf(s->manual_raid_gym,sizeof(s->manual_raid_gym),"%s",gym);s->manual_raid_seed=current;}
    if(strcmp(gym,s->manual_raid_gym)){manual_raid_release(s);return;}
    if(current!=s->manual_raid_seed){manual_raid_release(s);return;}
    if(!target(s,s->manual_raid_gui,&service)||!read_ref(s,service,"TopGui",&gui)){manual_raid_gate(s,4);return;}
    const char *name=s->api.class_name(s->base.object_get_class(gui));if(!name||strcmp(name,"RaidResolveUIController")){manual_raid_gate(s,5);return;}
    void *info=NULL,*reward=NULL,*state=NULL,*state_root=NULL,*pokemon=NULL;int32_t status=0,balls=0;int64_t encounter=0;
    if(!object_field(s,gui,"elwb","Niantic.Holoholo.Raid.IRaidResolveInformation",&info)||!exact(s,info,klass(s,"holo-game.dll","Niantic.Holoholo.Raid","RaidResolveInformation"))||!read_i32(s,info,"<Status>k__BackingField",&status,true)||status!=2){manual_raid_gate(s,6);return;}
    if(!object_field(s,gui,"elvp","Holoholo.Rpc.RaidEncounterProto",&reward)){
        void *results=NULL;if(!read_ref(s,info,"get_BattleResults",&results)||!ar_single_reward(s,results,&reward)){manual_raid_gate(s,7);return;}
    }
    if(!object_field(s,reward,"FortId","System.String",&fort)||!label_string(s,fort,gym,sizeof(gym))||strcmp(gym,s->manual_raid_gym)||!read_i64(s,reward,"EncounterId",&encounter)||!encounter||!object_field(s,reward,"Pokemon","Holoholo.Rpc.PokemonProto",&pokemon)||!read_i32(s,reward,"ThrowsRemaining",&balls,false)||balls<1||balls>200){manual_raid_gate(s,8);return;}
    if(!object_field(s,gui,"eluf","Niantic.Holoholo.Raid.RaidState",&state)||!object_field(s,state,"elwf","Niantic.Holoholo.Gym.IGymRootController",&state_root)||state_root!=root){manual_raid_gate(s,10);return;}
    bool active=false;if(!scalar(s,method(s,s->base.object_get_class(state),"get_IsActiveState","System.Boolean",0,NULL),state,&active,1)||!active){manual_raid_gate(s,11);return;}
    const char *types[]={"Holoholo.Rpc.RaidEncounterProto","Niantic.Holoholo.Raid.RaidResolveInformation"};
    const void *complete=method(s,s->base.object_get_class(state),"ExitGym","System.Void",2,types);if(!complete){manual_raid_gate(s,12);return;}
    if((s->config.flags&SILPH_ENH_INFO)&&keep(s,pokemon,&s->raid_display_pokemon,false))s->raid_display_encounter=(uint64_t)encounter;
    void *args[]={reward,info};s->manual_raid_until=0;
    bool ok=invoke(s,complete,state,args,NULL);__android_log_print(ANDROID_LOG_INFO,"SilphEnhancer","raid results advance encounter=%llu balls=%d accepted=%u; capture remains manual",(unsigned long long)(uint64_t)encounter,balls,ok);
    manual_raid_release(s);
}
