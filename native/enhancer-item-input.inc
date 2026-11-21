static void manual_input_release(Enhancer *s){
    drop(s,&s->throw_input);drop(s,&s->throw_input_owner);s->throw_input_stack=NULL;s->throw_input_collect=0;s->throw_input_id=s->throw_input_serial=s->throw_input_at=0;
}
static void *push_input(void *o,void *layer,const void *m){
    Enhancer *s=owned();ENH_SCOPE(s);bool collect=s&&s->throw_input_collect&&s->throw_input_stack==o&&!layer;
    void *receipt=push_input_original(o,layer,m);
    if(collect&&receipt&&!s->throw_input.serial&&keep(s,receipt,&s->throw_input,false))__android_log_print(ANDROID_LOG_INFO,"SilphEnhancer","raid throw input retained encounter=%llu",(unsigned long long)s->throw_input_id);
    return receipt;
}
static void manual_input_open(Enhancer *s){
    if(s->config.flags&SILPH_ENH_AUTORAID)return;void *k=klass(s,"holo-game-core.dll","Niantic.Holoholo.GameInput","InputLayerStack");const char *args[]={"Niantic.Holoholo.GameInput.IInputLayer"};
    const void *entry=method(s,k,"PushLayer","Niantic.Promises.IReceipt",1,args);
    if(!entry||hook(s,entry,(void *)push_input,(void **)&push_input_original))__android_log_print(ANDROID_LOG_INFO,"SilphEnhancer","manual raid input recovery unavailable; original flow retained");
}
static void manual_input_begin(Enhancer *s,void *interaction){
    manual_input_release(s);if(!push_input_original||(s->config.flags&SILPH_ENH_AUTORAID))return;
    void *state=NULL,*map=NULL,*stack=NULL;
    if(!target(s,s->state,&state)||!read_ref(s,state,"get_MapPokemon",&map))return;
    const char *name=s->api.class_name(s->base.object_get_class(map));if(!name||strcmp(name,"RaidMapPokemon"))return;
    if(!object_field(s,interaction,"dtfh","Niantic.Holoholo.GameInput.IInputLayerStack",&stack)||!keep(s,interaction,&s->throw_input_owner,true))return;
    s->throw_input_id=s->last_id;s->throw_input_serial=s->throw_serial;s->throw_input_stack=stack;s->throw_input_collect=1;
}
static void manual_input_tick(Enhancer *s,uint64_t now){
    if(!s->throw_input.serial||!s->throw_input_at)return;
    if(s->last_id!=s->throw_input_id||s->throw_serial!=s->throw_input_serial||now>s->throw_input_at+UINT64_C(5000000000)){manual_input_release(s);return;}
    if(now<s->throw_input_at||s->promise.serial||s->berry_promise.serial)return;
    void *receipt=NULL,*owner=NULL,*interaction=NULL,*menu=NULL;bool complete=false,playing=true,active=false;int32_t state=0;
    if(!target(s,s->throw_input,&receipt)||!target(s,s->throw_input_owner,&owner)||!target(s,s->interaction,&interaction)||owner!=interaction){manual_input_release(s);return;}
    if(!scalar(s,method(s,s->base.object_get_class(receipt),"get_Completed","System.Boolean",0,NULL),receipt,&complete,1))return;
    if(complete){manual_input_release(s);return;}
    if(!scalar(s,method(s,s->interaction_class,"get_IsActiveState","System.Boolean",0,NULL),interaction,&active,1)||!active||!scalar(s,s->ball_in_play,interaction,&playing,1)||playing)return;
    if(object_field(s,interaction,"dtgs","Niantic.Holoholo.Encounter.IEncounterItemUseGuiController",&menu)&&(!read_i32(s,menu,"currentState",&state,true)||state!=0))return;
    bool ok=invoke(s,method(s,s->base.object_get_class(receipt),"Complete","System.Void",0,NULL),receipt,NULL,NULL);
    __android_log_print(ANDROID_LOG_INFO,"SilphEnhancer","raid throw input restored encounter=%llu accepted=%u",(unsigned long long)s->throw_input_id,ok);manual_input_release(s);
}
static void manual_item_input(Enhancer *s,uint64_t now){
    if(s->config.flags&SILPH_ENH_AUTORAID)return;
    void *interaction=NULL,*menu=NULL,*previous=NULL;
    if(!target(s,s->interaction,&interaction)||!object_field(s,interaction,"dtgs","Niantic.Holoholo.Encounter.IEncounterItemUseGuiController",&menu)||!unity_alive(s,menu)){
        drop(s,&s->item_menu);s->item_menu_at=0;s->item_menu_done=0;return;
    }
    if(!target(s,s->item_menu,&previous)||previous!=menu){
        if(!keep(s,menu,&s->item_menu,true))return;s->item_menu_at=now;s->item_menu_done=0;
    }
    if(s->item_menu_done||s->promise.serial||s->berry_promise.serial||now-s->item_menu_at<UINT64_C(500000000))return;
    bool playing=true,disabled=false;int32_t berry=0,item=0,state=-1;
    if(!scalar(s,s->ball_in_play,interaction,&playing,1)||playing||!scalar(s,s->active_berry,interaction,&berry,4)||berry<701||berry>708||!read_i32(s,menu,"item",&item,true)||berry!=item)return;
    const void *flag=field(s,s->base.object_get_class(menu),"InputEventsDisabled","System.Boolean");
    if(!flag||!read_i32(s,menu,"currentState",&state,true))return;s->api.get(menu,flag,&disabled);
    if(!disabled||(state!=0&&state!=3))return;
    if(state==3){if(!(s->config.flags&SILPH_CFG_SCENES)||!invoke(s,method(s,s->base.object_get_class(menu),"OnDismissComplete","System.Void",0,NULL),menu,NULL,NULL))return;s->api.get(menu,flag,&disabled);}
    if(!disabled||invoke(s,method(s,s->base.object_get_class(menu),"ReenableInput","System.Void",0,NULL),menu,NULL,NULL)){
        s->item_menu_done=1;__android_log_print(ANDROID_LOG_INFO,"SilphEnhancer","manual berry input restored item=%d guiState=%d",berry,state);
    }
}
