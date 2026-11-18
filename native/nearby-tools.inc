static void (*nearby_init_original[2])(void *,const void *);
static void (*nearby_click_original[3])(void *,const void *);
static bool nearby_enabled(Enhancer *s){return allowed(s)&&(s->config.flags&SILPH_ENH_TELEPORT)&&!(s->config.flags&SILPH_ENH_AUTORAID)&&!s->promise.serial&&!s->raid.serial;}
static void nearby_init_legacy(void *o,const void *m){Enhancer *s=owned();ENH_SCOPE(s);nearby_init_original[0](o,m);if(nearby_enabled(s))keep(s,o,&s->nearby_gui,true);}
static void nearby_init_current(void *o,const void *m){Enhancer *s=owned();ENH_SCOPE(s);nearby_init_original[1](o,m);if(nearby_enabled(s))keep(s,o,&s->nearby_gui,true);}
static bool nearby_location(Enhancer *s,void *place,EnhLocation *location){return place&&scalar(s,method(s,s->base.object_get_class(place),"get_Location","Niantic.LatLng",0,NULL),place,location,sizeof(*location));}
static bool nearby_pokemon(Enhancer *s,void *icon,EnhLocation *location){
    void *id=NULL,*container=NULL,*entities=NULL,*place=NULL;if(!s->nearby_game||!s->nearby_scope||!s->nearby_entities_class||!read_ref(s,icon,"get_FortId",&id))return false;
    SilphManagedRef ref={0};int result=silph_game_read_raid_service(s->nearby_game,5,&ref);bool found=false;
    if(!result&&!silph_managed_target(s->nearby_scope,ref,&container)){
        void *type_object=s->api.type_object(s->base.class_get_type(s->nearby_entities_class));const char *type_arg[]={"System.Type"};void *resolve_args[]={type_object};
        if(type_object&&invoke(s,method(s,s->base.object_get_class(container),"TryResolve",NULL,1,type_arg),container,resolve_args,&entities)&&entities){const char *types[]={"System.String"};void *args[]={id};found=invoke(s,method(s,s->base.object_get_class(entities),"GetMapPoi",NULL,1,types),entities,args,&place)&&nearby_location(s,place,location);}
    }
    if(ref.serial)silph_managed_release(s->nearby_scope,ref);if(!found)__android_log_print(ANDROID_LOG_INFO,"SilphEnhancer","nearby Pokemon location unavailable service=%u place=%u error=%d",entities!=NULL,place!=NULL,result);return found;
}
static bool nearby_raid(Enhancer *s,void *icon,EnhLocation *location){
    void *fort=NULL;if(!read_ref(s,icon,"get_FortProto",&fort))return false;void *k=s->base.object_get_class(fort);const void *lat=field(s,k,"Latitude","System.Double"),*lon=field(s,k,"Longitude","System.Double");
    if(!lat||!lon)return false;s->api.get(fort,lat,&location->latitude);s->api.get(fort,lon,&location->longitude);return true;
}
static bool nearby_station(Enhancer *s,void *icon,EnhLocation *location){void *place=NULL;return read_ref(s,icon,"get_Station",&place)&&nearby_location(s,place,location);}
static bool nearby_jump(Enhancer *s,void *icon,unsigned kind){
    if(!nearby_enabled(s))return false;EnhLocation location={0};bool found=kind==0?nearby_pokemon(s,icon,&location):kind==1?nearby_raid(s,icon,&location):nearby_station(s,icon,&location);
    if(!found||!isfinite(location.latitude)||!isfinite(location.longitude)||fabs(location.latitude)>90||fabs(location.longitude)>180||(!location.latitude&&!location.longitude))return false;
    uint64_t now=clock_ns();if(s->nearby_at&&now-s->nearby_at<UINT64_C(400000000))return true;s->nearby_at=now;
    s->metrics.latitude=location.latitude;s->metrics.longitude=location.longitude;s->metrics.jumps++;
    void *gui=NULL;if(target(s,s->nearby_gui,&gui)&&unity_alive(s,gui)){const void *close=method(s,s->base.object_get_class(gui),"DismissWithPermissions",NULL,0,NULL);if(!close)close=method(s,s->base.object_get_class(gui),"Close",NULL,0,NULL);invoke(s,close,gui,NULL,NULL);}
    __android_log_print(ANDROID_LOG_INFO,"SilphEnhancer","nearby teleport kind=%u request=%llu lat=%.6f lon=%.6f",kind,(unsigned long long)s->metrics.jumps,location.latitude,location.longitude);return true;
}
static void nearby_click_pokemon(void *o,const void *m){Enhancer *s=owned();ENH_SCOPE(s);if(!nearby_jump(s,o,0))nearby_click_original[0](o,m);}
static void nearby_click_raid(void *o,const void *m){Enhancer *s=owned();ENH_SCOPE(s);if(!nearby_jump(s,o,1))nearby_click_original[1](o,m);}
static void nearby_click_station(void *o,const void *m){Enhancer *s=owned();ENH_SCOPE(s);if(!nearby_jump(s,o,2))nearby_click_original[2](o,m);}
static void nearby_open(Enhancer *s){
    if(!(s->config.flags&SILPH_ENH_TELEPORT)||(s->config.flags&SILPH_ENH_AUTORAID))return;
    s->nearby_entities_class=klass(s,"holo-game.dll","Niantic.Holoholo.Map","IMapEntityService");
    const char *space="Niantic.Holoholo.NearbyPokemon",*guis[]={"NearbyPokemonGuiController","NearbyPokemonGuiControllerV2"},*icons[]={"NearbyPokemonIcon","NearbyRaidIconWidget","NearbyStationIconWidget"},*clicks[]={"IconClicked","IconClicked","IconClickedAction"};
    void *initializers[]={(void *)nearby_init_legacy,(void *)nearby_init_current},*handlers[]={(void *)nearby_click_pokemon,(void *)nearby_click_raid,(void *)nearby_click_station};unsigned ready=0;
    for(unsigned i=0;i<2;i++){void *k=klass(s,"holo-game.dll",space,guis[i]);hook(s,method(s,k,"Initialize","System.Void",0,NULL),initializers[i],(void **)&nearby_init_original[i]);}
    for(unsigned i=0;i<3;i++){void *k=klass(s,"holo-game.dll",space,icons[i]);if(!hook(s,method(s,k,clicks[i],"System.Void",0,NULL),handlers[i],(void **)&nearby_click_original[i]))ready|=1u<<i;}
    __android_log_print(ANDROID_LOG_INFO,"SilphEnhancer","nearby shortcuts ready mask=%u",ready);
}
