static const void *static_method(Enhancer *s,void *k,const char *name,const char *result,unsigned count,const char **args){
    const void *m=k?s->api.method(k,name,(int)count):NULL;uint32_t impl=0;
    if(!m||!(s->base.method_get_flags(m,&impl)&0x10u)||s->base.method_get_param_count(m)!=count||s->base.method_is_generic(m)||s->base.method_is_inflated(m)||!type(s,s->base.method_get_return_type(m),result))return NULL;
    for(unsigned i=0;i<count;i++)if(!type(s,s->api.parameter(m,i),args[i]))return NULL;return m;
}
static bool read_ref(Enhancer *s,void *o,const char *name,void **out){
    *out=NULL;if(!o)return false;const void *m=method(s,s->base.object_get_class(o),name,NULL,0,NULL);return invoke(s,m,o,NULL,out)&&*out;
}
static bool read_i32(Enhancer *s,void *o,const char *name,int32_t *out,bool enumeration){
    if(!o)return false;void *k=s->base.object_get_class(o);const void *f=enumeration?enum_field(s,k,name):field(s,k,name,"System.Int32");if(!f)return false;s->api.get(o,f,out);return true;
}
static bool read_i64(Enhancer *s,void *o,const char *name,int64_t *out){
    const void *f=o?field(s,s->base.object_get_class(o),name,"System.Int64"):NULL;if(!f)return false;s->api.get(o,f,out);return true;
}
static void *new_value(Enhancer *s,void *k){
    void *o=k?s->api.object_new(k):NULL;return o&&invoke(s,method(s,k,".ctor","System.Void",0,NULL),o,NULL,NULL)?o:NULL;
}
static bool object_field(Enhancer *s,void *o,const char *name,const char *expected,void **out){
    *out=NULL;const void *f=o?field(s,s->base.object_get_class(o),name,expected):NULL;if(!f)return false;s->api.get(o,f,out);return *out!=NULL;
}
static bool label_string(Enhancer *s,void *string,char *out,size_t capacity){
    if(!string||capacity<2)return false;int n=s->base.string_length(string);const uint16_t *data=s->api.chars(string);if(n<0||n>256||!data)return false;size_t used=0;
    for(int i=0;i<n;i++){uint32_t c=data[i];if(c>=0xd800&&c<=0xdbff&&i+1<n&&data[i+1]>=0xdc00&&data[i+1]<=0xdfff)c=0x10000+((c-0xd800)<<10)+(data[++i]-0xdc00);
        unsigned bytes=c<0x80?1:c<0x800?2:c<0x10000?3:4;if(used+bytes>=capacity)return false;
        if(bytes==1)out[used++]=(char)c;else{for(unsigned j=bytes-1;j;j--)out[used++]=(char)((j==bytes-1?(bytes==2?0xc0:bytes==3?0xe0:0xf0):0x80)|(c>>(6*j)&(j==bytes-1?(bytes==2?31:bytes==3?15:7):63)));out[used++]=(char)(0x80|(c&63));}}
    out[used]=0;return true;
}
static float encounter_level(Enhancer *s,void *master,float multiplier){
    if(!master||!s->level_at||!isfinite(multiplier)||multiplier<=0)return 0;
    if(!s->level_ready){float first=0;int32_t index=1;void *args[]={master,&index},*boxed=NULL,*exception=NULL;boxed=s->base.runtime_invoke(s->level_at,NULL,args,&exception);if(exception||!boxed)return 0;memcpy(&first,s->base.object_unbox(boxed),4);
        int offset=first>.15f?1:0;s->level_count=0;
        for(unsigned level=1;level<=60;level++){index=(int32_t)level-offset;exception=NULL;boxed=s->base.runtime_invoke(s->level_at,NULL,args,&exception);float cp=0;if(exception||!boxed)break;void *raw=s->base.object_unbox(boxed);if(!raw)break;memcpy(&cp,raw,4);if(!isfinite(cp)||cp<=0||(level>1&&cp<=s->level_cp[level-1]))break;s->level_cp[level]=cp;s->level_count=level;}
        s->level_ready=s->level_count>=2;if(s->level_ready)__android_log_print(ANDROID_LOG_INFO,"SilphEnhancer","level table ready entries=%u first=%.7f",s->level_count,s->level_cp[1]);
    }
    float best=0,error=1;for(unsigned i=1;i<=s->level_count;i++){float diff=fabsf(multiplier-s->level_cp[i]);if(diff<error){error=diff;best=(float)i;}if(i<s->level_count){float half=sqrtf((s->level_cp[i]*s->level_cp[i]+s->level_cp[i+1]*s->level_cp[i+1])*.5f);diff=fabsf(multiplier-half);if(diff<error){error=diff;best=i+.5f;}}}return error<.0001f?best:0;
}
static void *encounter_pokemon(Enhancer *s,void *map){
    if(!map)return NULL;
    const char *responses[]={"EncounterOutProto","IncenseEncounterOutProto","DiskEncounterOutProto","QuestEncounterOutProto","TappableEncounterProto","InvasionEncounterOutProto","RaidEncounterProto"};
    void *k=s->base.object_get_class(map);uint64_t encounter=0;if(!scalar(s,method(s,k,"get_EncounterId","System.UInt64",0,NULL),map,&encounter,8)||!encounter)return NULL;
    if(s->raid_display_encounter==encounter){void *pokemon=NULL;if(target(s,s->raid_display_pokemon,&pokemon)&&exact(s,pokemon,s->pokemon_class))return pokemon;}
    for(unsigned depth=0;k&&depth<10;depth++,k=s->api.parent(k)){void *it=NULL;for(unsigned n=0;n<128;n++){const void *f=s->api.fields(k,&it);if(!f)break;if(s->api.field_flags(f)&0x50u)continue;void *response_class=s->api.class_from_type(s->api.field_type(f));const char *name=response_class?s->api.class_name(response_class):NULL;if(!name)continue;
        for(unsigned kind=0;kind<7;kind++){if(strcmp(name,responses[kind]))continue;char expected[96];snprintf(expected,sizeof(expected),"Holoholo.Rpc.%s",responses[kind]);if(!type(s,s->api.field_type(f),expected))continue;void *response=NULL;s->api.get(map,f,&response);if(!exact(s,response,response_class))continue;
            int32_t status=0;if(kind!=6&&(!read_i32(s,response,kind==0||kind==5?"Status":"Result",&status,true)||status!=1))continue;
            void *pokemon=NULL;if(kind==0){void *wild=NULL;uint64_t id=0;if(!object_field(s,response,"Pokemon","Holoholo.Rpc.WildPokemonProto",&wild))continue;const void *id_field=field(s,s->base.object_get_class(wild),"EncounterId","System.UInt64");if(!id_field)continue;s->api.get(wild,id_field,&id);if(id!=encounter||!object_field(s,wild,"Pokemon","Holoholo.Rpc.PokemonProto",&pokemon))continue;}
            else{if(kind==6){int64_t id=0;if(!read_i64(s,response,"EncounterId",&id)||(uint64_t)id!=encounter)continue;}if(!object_field(s,response,kind==5?"EncounterPokemon":"Pokemon","Holoholo.Rpc.PokemonProto",&pokemon))continue;}
            if(exact(s,pokemon,s->pokemon_class))return pokemon;
        }
    }}return NULL;
}
static bool display_instance(Enhancer *s,void *object,void *base){return object&&base&&s->api.assignable(base,s->base.object_get_class(object))&&unity_alive(s,object);}
static bool display_reference(Enhancer *s,void *object,const char *expected,void *base,void **out){
    *out=NULL;void *k=s->base.object_get_class(object);for(unsigned depth=0;k&&depth<12;depth++,k=s->api.parent(k)){void *it=NULL;for(unsigned i=0;i<128;i++){const void *f=s->api.fields(k,&it);if(!f)break;if(s->api.field_flags(f)&0x50u)continue;
        void *fk=s->api.class_from_type(s->api.field_type(f));if((expected&&type(s,s->api.field_type(f),expected))||(base&&fk&&s->api.assignable(base,fk))){s->api.get(object,f,out);if(*out)return true;}
    }}return false;
}
static bool display_render(Enhancer *s,void *o,void *map){
    if(!(s->metrics.available&SILPH_ENH_INFO)||!display_instance(s,o,s->nameplate_class)){s->display_stage=5;return false;}
    void *pokemon=NULL,*text=NULL,*original=NULL,*master=NULL;s->api.get(o,s->nameplate_text,&text);s->api.get(o,s->nameplate_master,&master);
    if(!display_instance(s,text,s->text_class)){s->display_stage=6;return false;}if(!(pokemon=encounter_pokemon(s,map))||!exact(s,pokemon,s->pokemon_class)){s->display_stage=7;return false;}if(!invoke(s,s->text_get,text,NULL,&original)||!original){s->display_stage=8;return false;}
    int32_t iv[3]={0};const char *getters[]={"get_IndividualAttack","get_IndividualDefense","get_IndividualStamina"};for(unsigned i=0;i<3;i++)if(!scalar(s,method(s,s->pokemon_class,getters[i],"System.Int32",0,NULL),pokemon,&iv[i],4)||iv[i]<0||iv[i]>15)return false;
    float cp=0,extra=0;if(!scalar(s,method(s,s->pokemon_class,"get_CpMultiplier","System.Single",0,NULL),pokemon,&cp,4)||!scalar(s,method(s,s->pokemon_class,"get_AdditionalCpMultiplier","System.Single",0,NULL),pokemon,&extra,4))return false;
    for(unsigned i=0;i<s->label_count;i++){void *prior=NULL,*rendered=NULL,*saved=NULL;if(target(s,s->labels[i].text,&prior)&&prior==text&&target(s,s->labels[i].rendered,&rendered)&&rendered==original&&target(s,s->labels[i].original,&saved)){original=saved;break;}}
    float level=encounter_level(s,master,cp+extra);char name[512],value[768],level_text[32];if(!label_string(s,original,name,sizeof(name)))return false;
    if(level>0)snprintf(level_text,sizeof(level_text),"Lv %.1f",level);else snprintf(level_text,sizeof(level_text),"Lv —");
    float percent=(iv[0]+iv[1]+iv[2])*100.f/45.f;bool perfect=iv[0]==15&&iv[1]==15&&iv[2]==15;
    int n=snprintf(value,sizeof(value),"%s\n<size=30><b>%s%s · %.1f%% · %d/%d/%d%s</b></size>",name,perfect?"<color=#FF4545>":"",level_text,percent,iv[0],iv[1],iv[2],perfect?"</color>":"");if(n<0||(size_t)n>=sizeof(value))return false;
    EnhLabel *record=NULL,*vacant=NULL;for(unsigned i=0;i<s->label_count;i++){void *prior=NULL;if(target(s,s->labels[i].text,&prior)&&unity_alive(s,prior)){if(prior==text){record=&s->labels[i];break;}}else if(!vacant)vacant=&s->labels[i];}
    if(!record){if(vacant){record=vacant;drop(s,&record->text);drop(s,&record->original);drop(s,&record->rendered);*record=(EnhLabel){0};}else{if(s->label_count>=12)return false;record=&s->labels[s->label_count++];}if(!keep(s,text,&record->text,true))return false;scalar(s,s->rich_get,text,&record->rich,1);scalar(s,s->overflow_get,text,&record->overflow,4);}
    void *rendered=s->base.string_new_len(value,(uint32_t)n);if(!rendered||!keep(s,original,&record->original,false)||!keep(s,rendered,&record->rendered,false))return false;
    bool rich=true;int32_t overflow=1;void *a[]={&rich},*b[]={&overflow},*c[]={rendered};invoke(s,s->rich_set,text,a,NULL);invoke(s,s->overflow_set,text,b,NULL);invoke(s,s->text_set,text,c,NULL);
    __android_log_print(ANDROID_LOG_INFO,"SilphEnhancer","encounter display iv=%d/%d/%d percent=%.1f level=%.1f",iv[0],iv[1],iv[2],percent,level);
    return true;
}
static void nameplate(void *o,void *map,int32_t weather,const void *m){
    Enhancer *s=owned();ENH_SCOPE(s);nameplate_original(o,map,weather,m);if(!allowed(s)||!(s->metrics.available&SILPH_ENH_INFO)||!display_instance(s,o,s->nameplate_class)||!map)return;
    keep(s,o,&s->display_plate,true);keep(s,map,&s->display_map,true);uint64_t id=0;scalar(s,method(s,s->base.object_get_class(map),"get_EncounterId","System.UInt64",0,NULL),map,&id,8);
    if(display_render(s,o,map))s->display_seen=id;else{s->display_seen=0;s->display_target=id;s->display_reported=0;s->display_until=clock_ns()+UINT64_C(5000000000);s->display_after=0;const char *kind=s->api.class_name(s->base.object_get_class(map));__android_log_print(ANDROID_LOG_INFO,"SilphEnhancer","encounter display pending map=%s raidReward=%u",kind?kind:"unknown",s->raid_display_encounter==id&&id!=0);}
}
static void display_tick(Enhancer *s,uint64_t now){
    if(!(s->metrics.available&SILPH_ENH_INFO)||!s->last_id||s->display_seen==s->last_id||now<s->display_after)return;
    if(s->display_target!=s->last_id){s->display_target=s->last_id;s->display_until=now+UINT64_C(5000000000);s->display_reported=0;}s->display_after=now+UINT64_C(200000000);
    if(now>s->display_until){s->display_seen=s->last_id;__android_log_print(ANDROID_LOG_INFO,"SilphEnhancer","encounter display unavailable stage=%u encounter=%llu raidReward=%u",s->display_stage,(unsigned long long)s->last_id,s->raid_display_encounter==s->last_id);return;}
    void *state=NULL,*map=NULL,*plate=NULL,*interaction=NULL,*ui=NULL,*previous=NULL;
    s->display_stage=1;if(!target(s,s->state,&state))return;s->display_stage=2;if(!invoke(s,s->state_map,state,NULL,&map)||!map)return;
    if(!target(s,s->display_map,&previous)||previous!=map||!target(s,s->display_plate,&plate)||!display_instance(s,plate,s->nameplate_class)){
        s->display_stage=3;if(!target(s,s->interaction,&interaction))return;s->display_stage=4;
        if(!read_ref(s,interaction,"get_EncounterUI",&ui)&&!display_reference(s,interaction,"Niantic.Holoholo.Encounter.IEncounterGuiController",NULL,&ui))return;
        if(!display_reference(s,ui,NULL,s->nameplate_class,&plate)||!display_instance(s,plate,s->nameplate_class)){
            if(!s->display_reported){s->display_reported=1;__android_log_print(ANDROID_LOG_INFO,"SilphEnhancer","encounter label GUI class=%s",s->api.class_name(s->base.object_get_class(ui)));}
            s->display_stage=5;return;
        }
        keep(s,plate,&s->display_plate,true);keep(s,map,&s->display_map,true);
    }
    if(display_render(s,plate,map))s->display_seen=s->last_id;
}
static void display_open(Enhancer *s){
    if(!(s->config.flags&SILPH_ENH_INFO))return;s->nameplate_class=klass(s,"holo-game.dll","Niantic.Holoholo.Encounter","EncounterNameplate");s->text_class=klass(s,"UnityEngine.UI.dll","UnityEngine.UI","Text");s->pokemon_class=klass(s,"holo-protos.dll","Holoholo.Rpc","PokemonProto");
    s->nameplate_text=field(s,s->nameplate_class,"nameText","UnityEngine.UI.Text");s->nameplate_master=field(s,s->nameplate_class,"dufa","IGameMasterData");s->nameplate_pokemon=s->pokemon_class;
    s->text_get=method(s,s->text_class,"get_text","System.String",0,NULL);const char *str[]={"System.String"},*bit[]={"System.Boolean"},*over[]={"UnityEngine.VerticalWrapMode"};s->text_set=method(s,s->text_class,"set_text","System.Void",1,str);s->rich_get=method(s,s->text_class,"get_supportRichText","System.Boolean",0,NULL);s->rich_set=method(s,s->text_class,"set_supportRichText","System.Void",1,bit);s->overflow_get=method(s,s->text_class,"get_verticalOverflow","UnityEngine.VerticalWrapMode",0,NULL);s->overflow_set=method(s,s->text_class,"set_verticalOverflow","System.Void",1,over);
    const char *level[]={"IGameMasterData","System.Int32"};s->level_at=static_method(s,klass(s,"holo-game-core.dll","Niantic.Holoholo","GameMasterDataExt_PlayerLevelSettings"),"CpMultiplierAtLevel","System.Single",2,level);
    const void *entry=s->nameplate_class?s->api.method(s->nameplate_class,"SetPokemonUI",2):NULL;
    if(entry){void *weather=s->api.class_from_type(s->api.parameter(entry,1));const char *name=weather?s->api.class_name(weather):NULL;if(s->base.method_get_param_count(entry)!=2||!type(s,s->base.method_get_return_type(entry),"System.Void")||!type(s,s->api.parameter(entry,0),"Niantic.Holoholo.Map.IMapPokemon")||!name||strcmp(name,"WeatherCondition")||!s->api.is_enum(weather)||s->base.type_get_type(s->api.enum_base(weather))!=8)entry=NULL;}
    __android_log_print(ANDROID_LOG_INFO,"SilphEnhancer","encounter contract entry=%u class=%u text=%u pokemon=%u master=%u get=%u set=%u rich=%u/%u overflow=%u/%u level=%u",entry!=NULL,s->text_class!=NULL,s->nameplate_text!=NULL,s->nameplate_pokemon!=NULL,s->nameplate_master!=NULL,s->text_get!=NULL,s->text_set!=NULL,s->rich_get!=NULL,s->rich_set!=NULL,s->overflow_get!=NULL,s->overflow_set!=NULL,s->level_at!=NULL);
    if(entry&&s->nameplate_text&&s->nameplate_master&&s->nameplate_pokemon&&s->text_get&&s->text_set&&s->rich_get&&s->rich_set&&s->overflow_get&&s->overflow_set&&s->level_at&&!hook(s,entry,(void *)nameplate,(void **)&nameplate_original))s->metrics.available|=SILPH_ENH_INFO;
}
static void display_close(Enhancer *s){
    drop(s,&s->display_plate);drop(s,&s->display_map);drop(s,&s->raid_display_pokemon);
    for(unsigned i=0;i<s->label_count;i++){EnhLabel *record=&s->labels[i];void *text=NULL,*original=NULL,*rendered=NULL,*actual=NULL;if(target(s,record->text,&text)&&display_instance(s,text,s->text_class)&&target(s,record->original,&original)&&target(s,record->rendered,&rendered)&&invoke(s,s->text_get,text,NULL,&actual)&&actual==rendered){void *a[]={original},*b[]={&record->rich},*c[]={&record->overflow};invoke(s,s->text_set,text,a,NULL);invoke(s,s->rich_set,text,b,NULL);invoke(s,s->overflow_set,text,c,NULL);}drop(s,&record->text);drop(s,&record->original);drop(s,&record->rendered);}
}
