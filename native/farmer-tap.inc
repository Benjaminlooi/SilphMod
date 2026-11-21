typedef struct {float x,y;} FarmerTapPoint;
typedef struct {double latitude,longitude;} FarmerTapLocation;
static bool tap_type(View *s,const void *t,const char *expected){const char *name=t?s->api.type_name(t):NULL;bool equal=name&&!strcmp(name,expected);if(name)s->api.free((void *)name);return equal;}
static const void *tap_method(View *s,void *k,const char *name,const char *result,unsigned count,const char **args){
    const void *m=k?s->api.method(k,name,(int)count):NULL;uint32_t impl=0;
    if(!m||s->base.method_get_param_count(m)!=count||(s->base.method_get_flags(m,&impl)&0x10u)||s->base.method_is_generic(m)||!tap_type(s,s->base.method_get_return_type(m),result))return NULL;
    for(unsigned i=0;i<count;i++)if(!tap_type(s,s->api.parameter(m,i),args[i]))return NULL;return m;
}
static bool tap_object(View *s,void *object,void *k){return k&&silph_callback_object(object,k)&&s->base.object_get_class(object)==k;}
static void map_tap(void *object,void *sender,void *event,const void *m){
    tap_original(object,sender,event,m);if(atomic_load(&owner_tid)!=(unsigned)syscall(__NR_gettid))return;View *s=atomic_load(&current);
    if(!s||s->closed||s->busy||!s->tap_enabled||!s->allowed(s->allowed_context)||!tap_object(s,object,s->tap_class))return;
    void *map=NULL,*gui=NULL,*top=NULL;bool active=false,visible=false;
    if(silph_managed_target(s->managed,s->map,&map)||!map||boolean(s,s->active,map,&active)||!active||boolean(s,s->visible,map,&visible)||!visible)return;
    if(silph_managed_target(s->managed,s->gui,&gui)||invoke(s,s->top,gui,&top)||!top||s->base.object_get_class(top)!=s->main_menu){s->tap_at=0;return;}
    void *gesture=NULL,*controller=NULL;s->api.get(object,s->tap_gesture,&gesture);s->api.get(object,s->tap_controller,&controller);
    if(!tap_object(s,gesture,s->gesture_class)||!controller)return;
    FarmerTapPoint point={0};s->api.get(gesture,s->tap_position,&point);if(!isfinite(point.x)||!isfinite(point.y))return;
    uint64_t now=clock_ns();float x=point.x-s->tap_x,y=point.y-s->tap_y;
    bool twice=s->tap_at&&now>s->tap_at&&now-s->tap_at>=UINT64_C(50000000)&&now-s->tap_at<=UINT64_C(350000000)&&x*x+y*y<=6400;
    s->tap_at=now;s->tap_x=point.x;s->tap_y=point.y;if(!twice)return;s->tap_at=0;
    const char *types[]={"UnityEngine.Vector2","Niantic.LatLng&"};const void *convert=tap_method(s,s->base.object_get_class(controller),"GetGroundLocation","System.Boolean",2,types);
    FarmerTapLocation location={0};void *args[]={&point,&location},*exception=NULL,*boxed=convert?s->base.runtime_invoke(convert,controller,args,&exception):NULL;
    if(exception||!boxed)return;void *value=s->base.object_unbox(boxed);bool accepted=false;if(value)memcpy(&accepted,value,1);
    if(!accepted||!isfinite(location.latitude)||!isfinite(location.longitude)||fabs(location.latitude)>90||fabs(location.longitude)>180)return;
    s->metrics.latitude=location.latitude;s->metrics.longitude=location.longitude;s->metrics.taps++;
    __android_log_print(ANDROID_LOG_INFO,"SilphFarmer","map gesture accepted request=%llu",(unsigned long long)s->metrics.taps);
}
int silph_farmer_view_teleport(void *memory,bool enabled){
    View *s=memory;if(!s||s->closed||s->owner!=(unsigned)syscall(__NR_gettid))return 1;
    if(!enabled){s->tap_enabled=0;s->tap_at=0;return 0;}if(s->tap_checked){s->tap_enabled=s->metrics.tap_available;return s->metrics.tap_error?1:0;}s->tap_checked=1;s->tap_enabled=1;
    void *parameter=s->resolver("il2cpp_method_get_param",s->context);if(parameter)memcpy(&s->api.parameter,&parameter,sizeof(parameter));
    s->tap_class=klass(s,"holo-game.dll","Niantic.Holoholo.Map","GroundTapHandler");s->gesture_class=klass(s,"TouchScript.dll","TouchScript.Gestures","TapGesture");
    s->tap_gesture=field(s,s->tap_class,"tapGesture","TouchScript.Gestures.TapGesture");s->tap_controller=field(s,s->tap_class,"enoh","Niantic.Holoholo.Map.IMapController");s->tap_position=field(s,s->gesture_class,"startPosition","UnityEngine.Vector2");
    const char *types[]={"System.Object","System.EventArgs"};const void *entry=NULL;void *iterator=NULL;
    if(s->api.parameter&&s->tap_class)for(unsigned i=0;i<1024;i++){
        const void *candidate=s->base.class_get_methods(s->tap_class,&iterator);if(!candidate)break;const char *name=s->base.method_get_name(candidate);
        if(name&&tap_method(s,s->tap_class,name,"System.Void",2,types)==candidate){if(entry){entry=NULL;break;}entry=candidate;}
    }
    if(s->tap_gesture&&s->tap_controller&&s->tap_position&&entry){
        memcpy(&s->tap_address,(const char *)entry+s->offset,sizeof(s->tap_address));
        if(!(s->tap_address&3)&&s->address_check(s->tap_address,s->context)){memcpy(s->tap_before,(void *)s->tap_address,16);s->tap_stub=shadowhook_hook_func_addr((void *)s->tap_address,(void *)map_tap,(void **)&tap_original);}
    }
    s->metrics.tap_error=s->tap_stub?0:1;s->metrics.tap_available=s->tap_stub!=NULL;if(!s->tap_stub)s->tap_enabled=0;
    __android_log_print(ANDROID_LOG_INFO,"SilphFarmer","map gesture prepared available=%u",s->metrics.tap_available);return s->tap_stub?0:1;
}
