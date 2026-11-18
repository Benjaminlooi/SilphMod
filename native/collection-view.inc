#include "collection-names.h"
static void *(*collection_create_original)(void *,int32_t,void *,const void *);
static void (*collection_panel_original)(void *,void *,const void *),(*collection_reward_original)(void *,void *,const void *);
static bool collection_enabled(Enhancer *s,unsigned mask){return allowed(s)&&(s->config.collection_info&mask)&&!(s->config.flags&SILPH_ENH_AUTORAID);}
static const void *collection_method(Enhancer *s,void *k,const char *name,const char *result,unsigned count,const char **args){
    for(unsigned depth=0;k&&depth<8;depth++,k=s->api.parent(k)){void *it=NULL;const void *found=NULL;
        for(unsigned i=0;i<2048;i++){const void *m=s->base.class_get_methods(k,&it);if(!m)break;uint32_t impl=0;const char *actual=s->base.method_get_name(m);
            if(!actual||strcmp(actual,name)||s->base.method_get_param_count(m)!=count||(s->base.method_get_flags(m,&impl)&0x10u)||s->base.method_is_generic(m)||(result&&!type(s,s->base.method_get_return_type(m),result)))continue;
            bool same=true;for(unsigned j=0;j<count;j++)same=same&&type(s,s->api.parameter(m,j),args[j]);if(!same)continue;if(found)return NULL;found=m;
        }if(found)return found;
    }return NULL;
}
static const void *collection_static(Enhancer *s,void *k,const char *name,const char *result,unsigned count,const char **args){
    void *it=NULL;const void *found=NULL;for(unsigned i=0;k&&i<2048;i++){const void *m=s->base.class_get_methods(k,&it);if(!m)break;uint32_t impl=0;const char *actual=s->base.method_get_name(m);if(!actual||strcmp(actual,name)||s->base.method_get_param_count(m)!=count||!(s->base.method_get_flags(m,&impl)&0x10u)||s->base.method_is_generic(m)||s->base.method_is_inflated(m)||!type(s,s->base.method_get_return_type(m),result))continue;bool same=true;for(unsigned j=0;j<count;j++)same=same&&type(s,s->api.parameter(m,j),args[j]);if(!same)continue;if(found)return NULL;found=m;}return found;
}
static bool collection_invoke_static(Enhancer *s,const void *m,void **args,void **out){uint32_t impl=0;if(!m||!(s->base.method_get_flags(m,&impl)&0x10u))return false;void *exception=NULL,*result=s->base.runtime_invoke(m,NULL,args,&exception);if(out)*out=result;return !exception;}
static bool collection_bool(Enhancer *s,void *o,const char *name,bool *out){const void *f=o?field(s,s->base.object_get_class(o),name,"System.Boolean"):NULL;if(!f)return false;s->api.get(o,f,out);return true;}
static bool collection_u64(Enhancer *s,void *o,const char *name,uint64_t *out){const void *f=o?field(s,s->base.object_get_class(o),name,"System.UInt64"):NULL;if(!f)return false;s->api.get(o,f,out);return true;}
static bool collection_count(Enhancer *s,void *list,unsigned limit,uint32_t *out){
    int32_t n=0;if(!list||!scalar(s,collection_method(s,s->base.object_get_class(list),"get_Count","System.Int32",0,NULL),list,&n,4)||n<0||(unsigned)n>limit)return false;*out=(uint32_t)n;return true;
}
static bool collection_item(Enhancer *s,void *list,uint32_t index,void **out){
    const char *args[]={"System.Int32"};int32_t i=(int32_t)index;void *a[]={&i};return invoke(s,collection_method(s,s->base.object_get_class(list),"get_Item",NULL,1,args),list,a,out)&&*out;
}
static void collection_sources(Enhancer *s,void *o){
    if(!o)return;void *master=NULL,*player=NULL;target(s,s->collection.master,&master);target(s,s->collection.player,&player);
    if(!master&&!display_reference(s,o,"IGameMasterData",NULL,&master)&&s->collection.master_type){void *container=NULL,*reflection=s->api.type_object(s->base.class_get_type(s->collection.master_type));
        if(reflection&&display_reference(s,o,"Zenject.DiContainer",NULL,&container)){const char *types[]={"System.Type"};void *args[]={reflection};invoke(s,collection_method(s,s->base.object_get_class(container),"TryResolve",NULL,1,types),container,args,&master);}}
    if(master&&!s->collection.master.serial)keep(s,master,&s->collection.master,false);
    if(!player&&display_reference(s,o,"Niantic.Holoholo.IPlayerService",NULL,&player))keep(s,player,&s->collection.player,false);
}
static void collection_name(uint32_t species,uint32_t form,char *out,size_t bytes){
    const char *name=species<2048?collection_names[species]:NULL;
    for(unsigned i=0;i<sizeof(collection_forms)/sizeof(collection_forms[0]);i++)if(collection_forms[i].species==species&&collection_forms[i].form==form){name=collection_forms[i].name;break;}
    if(name)snprintf(out,bytes,"%s",name);else snprintf(out,bytes,"Pokémon %u",species);
}
static void collection_move_name(Enhancer *s,uint32_t id,char *out,size_t bytes){
    char raw[160]={0};void *it=NULL;CollectionView *v=&s->collection;
    if(v->move_enum&&v->field_name&&v->static_get)for(unsigned i=0;i<1024;i++){const void *f=s->api.fields(v->move_enum,&it);if(!f)break;if((s->api.field_flags(f)&0x50u)!=0x50u||!type(s,s->api.field_type(f),"Holoholo.Rpc.HoloPokemonMove"))continue;int32_t value=0;v->static_get(f,&value);if(value>=0&&(uint32_t)value==id){const char *name=v->field_name(f);if(name)snprintf(raw,sizeof(raw),"%s",name);break;}}
    if(!raw[0]){snprintf(out,bytes,"Move %u",id);return;}
    const char *label=raw;if(raw[0]=='V'){unsigned i=1;while(raw[i]>='0'&&raw[i]<='9')i++;if(i>1&&!strncmp(raw+i,"Move",4))label=raw+i+4;}
    size_t used=0;for(size_t i=0;label[i]&&used+2<bytes;i++){unsigned char c=(unsigned char)label[i];if(c=='_'){if(used&&out[used-1]!=' ')out[used++]=' ';continue;}if(i&&c>='A'&&c<='Z'&&label[i-1]>='a'&&label[i-1]<='z'&&used&&out[used-1]!=' ')out[used++]=' ';out[used++]=(char)c;}out[used]=0;
    if(used>5&&!strcmp(out+used-5," Fast"))out[used-5]=0;
}
static CollectionEntry *collection_entry(Enhancer *s,void *egg){
    bool is_egg=false;uint64_t id=0;int64_t created=0;if(!exact(s,egg,s->pokemon_class)||!collection_bool(s,egg,"isEgg_",&is_egg)||!is_egg||!collection_u64(s,egg,"id_",&id)||!id||!read_i64(s,egg,"creationTimeMs_",&created))return NULL;
    uint64_t now=clock_ns();CollectionEntry *entry=NULL;for(unsigned i=0;i<32;i++)if(s->collection.entries[i].id==id&&s->collection.entries[i].created==(uint64_t)created){entry=&s->collection.entries[i];break;}
    if(entry&&entry->ready)return entry;if(entry&&now<entry->retry)return NULL;if(!entry)entry=&s->collection.entries[s->collection.cursor++%32];*entry=(CollectionEntry){.id=id,.created=(uint64_t)created,.retry=now+UINT64_C(30000000000)};
    unsigned stage=1,matches=0;void *distribution=NULL,*list=NULL,*selected=NULL,*display=NULL;
    if(!object_field(s,egg,"eggDistribution_","Holoholo.Rpc.EggDistributionProto",&distribution)||!object_field(s,distribution,"EggDistribution","Niantic.Protobuf.Collections.RepeatedField<Holoholo.Rpc.EggDistributionProto.Types.EggDistributionEntryProto>",&list))goto unavailable;
    uint32_t n=0;stage=2;if(!collection_count(s,list,256,&n)||!n)goto unavailable;
    for(uint32_t i=0;i<n;i++){void *item=NULL,*appearance=NULL;int64_t seed=0;if(!collection_item(s,list,i,&item)||!object_field(s,item,"PokemonDisplay","Holoholo.Rpc.PokemonDisplayProto",&appearance)||!read_i64(s,appearance,"DisplayId",&seed))goto unavailable;
        uint64_t distance=seed>created?(uint64_t)seed-(uint64_t)created:(uint64_t)created-(uint64_t)seed;if(distance>10){matches++;selected=item;display=appearance;entry->seed=(uint64_t)seed;}}
    stage=3;if(matches!=1||!selected)goto unavailable;int32_t species=0,form=0;if(!read_i32(s,selected,"PokemonId",&species,true)||species<=0||species>=2048||!read_i32(s,display,"Form",&form,true)||form<0)goto unavailable;
    entry->species=(uint32_t)species;entry->form=(uint32_t)form;collection_name(entry->species,entry->form,entry->name,sizeof(entry->name));
    void *master=NULL,*player=NULL,*settings=NULL,*quick=NULL,*charged=NULL,*stats=NULL,*player_stats=NULL;stage=4;if(!target(s,s->collection.master,&master)||!target(s,s->collection.player,&player))goto unavailable;
    const char *setting_args[]={"Holoholo.Rpc.HoloPokemonId","Holoholo.Rpc.PokemonDisplayProto.Types.Form"};void *args[]={&species,&form};
    if(!invoke(s,collection_method(s,s->base.object_get_class(master),"Get_PokemonSettings",NULL,2,setting_args),master,args,&settings)||!exact(s,settings,s->collection.settings))goto unavailable;
    stage=5;if(!object_field(s,settings,"quickMoves_","Niantic.Protobuf.Collections.RepeatedField<Holoholo.Rpc.HoloPokemonMove>",&quick)||!object_field(s,settings,"cinematicMoves_","Niantic.Protobuf.Collections.RepeatedField<Holoholo.Rpc.HoloPokemonMove>",&charged)||!object_field(s,settings,"stats_","Holoholo.Rpc.PokemonStatsAttributesProto",&stats))goto unavailable;
    uint32_t fast_count=0,charged_count=0;if(!collection_count(s,quick,256,&fast_count)||!collection_count(s,charged,256,&charged_count)||!silph_collection_stats(entry->seed,fast_count,charged_count,&entry->stats))goto unavailable;
    stage=6;int32_t attack=0,defense=0,stamina=0,level=0;if(!read_i32(s,stats,"BaseAttack",&attack,false)||!read_i32(s,stats,"BaseDefense",&defense,false)||!read_i32(s,stats,"BaseStamina",&stamina,false))goto unavailable;
    if(!read_ref(s,player,"get_PlayerStats",&player_stats)||(!read_i32(s,player_stats,"level_",&level,false)&&!read_i32(s,player_stats,"Level",&level,false))||level<1||level>100)goto unavailable;
    entry->level=(uint32_t)(level>20?20:level);encounter_level(s,master,.094f);if(!s->level_ready||entry->level>s->level_count)goto unavailable;
    entry->cp=silph_collection_cp((uint32_t)attack,(uint32_t)defense,(uint32_t)stamina,&entry->stats,s->level_cp[entry->level]);if(!entry->cp)goto unavailable;
    stage=7;if(entry->stats.moves){void *fast_box=NULL,*charged_box=NULL;if(!collection_item(s,quick,entry->stats.fast,&fast_box)||!collection_item(s,charged,entry->stats.charged,&charged_box))goto unavailable;void *f=s->base.object_unbox(fast_box),*c=s->base.object_unbox(charged_box);if(!f||!c)goto unavailable;memcpy(&entry->fast,f,4);memcpy(&entry->charged,c,4);if(entry->fast&&entry->charged){collection_move_name(s,entry->fast,entry->moves[0],sizeof(entry->moves[0]));collection_move_name(s,entry->charged,entry->moves[1],sizeof(entry->moves[1]));}}
    entry->ready=1;__android_log_print(ANDROID_LOG_INFO,"SilphCollection","egg detail id=%llu seed=%llu species=%u form=%u iv=%u/%u/%u cp=%u level=%u size=%u moves=%u/%u",(unsigned long long)id,(unsigned long long)entry->seed,entry->species,entry->form,entry->stats.attack,entry->stats.defense,entry->stats.stamina,entry->cp,entry->level,entry->stats.size,entry->fast,entry->charged);return entry;
unavailable:
    __android_log_print(ANDROID_LOG_INFO,"SilphCollection","egg detail deferred id=%llu stage=%u matches=%u",(unsigned long long)id,stage,matches);return NULL;
}
static void collection_position(Enhancer *s,CollectionLabel *r,void *text,void *view){
    void *rect=NULL;if(!read_ref(s,text,"get_rectTransform",&rect))return;const char *point[]={"UnityEngine.Vector2"};const void *set=collection_method(s,s->base.object_get_class(rect),"set_anchoredPosition","System.Void",1,point);
    if(r->moved){void *args[]={&r->position};if(!invoke(s,set,rect,args,NULL))return;r->moved=0;}
    if(!view)return;EnhPoint position={0};if(!scalar(s,collection_method(s,s->base.object_get_class(rect),"get_anchoredPosition","UnityEngine.Vector2",0,NULL),rect,&position,sizeof(position))||!isfinite(position.x)||!isfinite(position.y))return;
    void *image=NULL;if(!object_field(s,view,"imageTransform","UnityEngine.Transform",&image))return;
    struct {float x,y,width,height;} bounds={0};struct {float x,y,z;} local={0},world={0};
    if(!scalar(s,collection_method(s,s->base.object_get_class(image),"get_rect","UnityEngine.Rect",0,NULL),image,&bounds,sizeof(bounds))||!isfinite(bounds.height)||bounds.height<=0)return;
    local.x=bounds.x+bounds.width*.5f;local.y=bounds.y+bounds.height*.70f+6;
    const char *vector[]={"UnityEngine.Vector3"};void *args[]={&local},*boxed=NULL;
    if(!invoke(s,collection_method(s,s->base.object_get_class(image),"TransformPoint","UnityEngine.Vector3",1,vector),image,args,&boxed)||!boxed)return;void *value=s->base.object_unbox(boxed);if(!value)return;memcpy(&world,value,sizeof(world));
    if(!isfinite(world.x)||!isfinite(world.y)||!isfinite(world.z))return;void *move[]={&world};if(invoke(s,collection_method(s,s->base.object_get_class(rect),"set_position","System.Void",1,vector),rect,move,NULL)){r->position=position;r->moved=1;}
}
static void collection_reward_position(Enhancer *s,void *text,void *view){
    if(!display_instance(s,text,s->text_class))return;for(unsigned i=0;i<s->collection.label_count;i++){void *old=NULL;if(target(s,s->collection.labels[i].label.text,&old)&&old==text){collection_position(s,&s->collection.labels[i],text,view);return;}}
}
static void collection_restore(Enhancer *s,CollectionLabel *r){
    EnhLabel *label=&r->label;void *text=NULL,*original=NULL,*rendered=NULL,*actual=NULL;if(target(s,label->text,&text)&&display_instance(s,text,s->text_class)&&target(s,label->original,&original)&&target(s,label->rendered,&rendered)&&invoke(s,s->text_get,text,NULL,&actual)&&actual==rendered){void *a[]={original},*b[]={&label->rich},*c[]={&label->overflow};invoke(s,s->text_set,text,a,NULL);invoke(s,s->rich_set,text,b,NULL);invoke(s,s->overflow_set,text,c,NULL);}
    if(target(s,label->text,&text)&&display_instance(s,text,s->text_class))collection_position(s,r,text,NULL);drop(s,&label->text);drop(s,&label->original);drop(s,&label->rendered);*r=(CollectionLabel){0};
}
static void collection_text(Enhancer *s,void *text,const char *value){
    if(!display_instance(s,text,s->text_class)||strlen(value)>=768)return;CollectionLabel *r=NULL,*vacant=NULL;void *actual=NULL;if(!invoke(s,s->text_get,text,NULL,&actual)||!actual)return;
    for(unsigned i=0;i<s->collection.label_count;i++){void *old=NULL;if(target(s,s->collection.labels[i].label.text,&old)&&display_instance(s,old,s->text_class)){if(old==text){r=&s->collection.labels[i];break;}}else if(!vacant)vacant=&s->collection.labels[i];}
    if(!r){if(vacant){r=vacant;collection_restore(s,r);}else{if(s->collection.label_count>=64)return;r=&s->collection.labels[s->collection.label_count++];}if(!keep(s,text,&r->label.text,true))return;scalar(s,s->rich_get,text,&r->label.rich,1);scalar(s,s->overflow_get,text,&r->label.overflow,4);}
    void *rendered=NULL;target(s,r->label.rendered,&rendered);if(actual!=rendered&&!keep(s,actual,&r->label.original,false))return;
    if(!rendered||strcmp(r->value,value)){rendered=s->base.string_new_len(value,(uint32_t)strlen(value));if(!rendered||!keep(s,rendered,&r->label.rendered,false))return;snprintf(r->value,sizeof(r->value),"%s",value);}
    if(actual==rendered)return;bool rich=true;int32_t overflow=1;void *a[]={&rich},*b[]={&overflow},*c[]={rendered};invoke(s,s->rich_set,text,a,NULL);invoke(s,s->overflow_set,text,b,NULL);invoke(s,s->text_set,text,c,NULL);
}
static const char *collection_size(const CollectionEntry *e){const char *sizes[]={"M","XXS","XS","XL","XXL"};return e->stats.size<5?sizes[e->stats.size]:"—";}
static void collection_badge_destroy(Enhancer *s,CollectionBadge *badge){
    void *text=NULL,*object=NULL;if(target(s,badge->text,&text)&&display_instance(s,text,s->text_class)&&read_ref(s,text,"get_gameObject",&object)){void *args[]={object};collection_invoke_static(s,s->collection.destroy,args,NULL);}drop(s,&badge->text);drop(s,&badge->view);
}
static void *collection_badge_deferred(Enhancer *s,unsigned stage){if(!s->collection.layout_reported++)__android_log_print(ANDROID_LOG_INFO,"SilphCollection","egg label deferred stage=%u clone=%u destroy=%u",stage,s->collection.instantiate!=NULL,s->collection.destroy!=NULL);return NULL;}
static void *collection_badge(Enhancer *s,void *view,void *source){
    CollectionBadge *badge=NULL,*vacant=NULL;for(unsigned i=0;i<32;i++){void *old=NULL,*text=NULL;if(target(s,s->collection.badges[i].view,&old)&&unity_alive(s,old)&&target(s,s->collection.badges[i].text,&text)&&display_instance(s,text,s->text_class)){if(old==view)return text;}else if(!vacant)vacant=&s->collection.badges[i];}
    if(!vacant||!s->collection.instantiate||!s->collection.destroy)return collection_badge_deferred(s,1);badge=vacant;collection_badge_destroy(s,badge);
    void *root=NULL,*clone=NULL;if(!read_ref(s,view,"get_transform",&root))return collection_badge_deferred(s,2);
    bool world=false;void *args[]={source,root,&world};if(!collection_invoke_static(s,s->collection.instantiate,args,&clone)||clone==source||!display_instance(s,clone,s->text_class))return collection_badge_deferred(s,3);
    if(!keep(s,view,&badge->view,true)||!keep(s,clone,&badge->text,true)){void *object=NULL;if(read_ref(s,clone,"get_gameObject",&object)){void *a[]={object};collection_invoke_static(s,s->collection.destroy,a,NULL);}drop(s,&badge->text);drop(s,&badge->view);return collection_badge_deferred(s,4);}
    void *rect=NULL;if(!read_ref(s,clone,"get_rectTransform",&rect)){collection_badge_destroy(s,badge);return collection_badge_deferred(s,5);}
    EnhPoint minimum={0,0},maximum={1,0},pivot={.5f,1.f},size={-8,78},offset={0,-2};const char *point[]={"UnityEngine.Vector2"};const char *names[]={"set_anchorMin","set_anchorMax","set_pivot","set_sizeDelta","set_anchoredPosition"};EnhPoint *values[]={&minimum,&maximum,&pivot,&size,&offset};
    for(unsigned i=0;i<5;i++){void *a[]={values[i]};if(!invoke(s,collection_method(s,s->base.object_get_class(rect),names[i],"System.Void",1,point),rect,a,NULL)){collection_badge_destroy(s,badge);return collection_badge_deferred(s,9+i);}}
    int32_t alignment=1,font_size=20;bool raycast=false;const char *text_anchor[]={"UnityEngine.TextAnchor"},*number[]={"System.Int32"},*bit[]={"System.Boolean"};void *a[]={&alignment},*b[]={&font_size},*c[]={&raycast};invoke(s,collection_method(s,s->text_class,"set_alignment","System.Void",1,text_anchor),clone,a,NULL);invoke(s,collection_method(s,s->text_class,"set_fontSize","System.Void",1,number),clone,b,NULL);invoke(s,collection_method(s,s->text_class,"set_raycastTarget","System.Void",1,bit),clone,c,NULL);
    invoke(s,collection_method(s,s->text_class,"set_resizeTextForBestFit","System.Void",1,bit),clone,c,NULL);
    if(!s->collection.layout_reported++)__android_log_print(ANDROID_LOG_INFO,"SilphCollection","egg list labels ready");return clone;
}
static void *collection_create(void *o,int32_t index,void *egg,const void *m){
    Enhancer *s=owned();ENH_SCOPE(s);void *view=collection_create_original(o,index,egg,m);if(!collection_enabled(s,1)||!view)return view;collection_sources(s,o);CollectionEntry *e=collection_entry(s,egg);if(!e)return view;
    void *source=NULL;if(!object_field(s,view,"kmTotalText","UnityEngine.UI.Text",&source)){collection_badge_deferred(s,14);return view;}void *text=collection_badge(s,view,source);if(!text)return view;bool perfect=e->stats.attack==15&&e->stats.defense==15&&e->stats.stamina==15;char value[512];snprintf(value,sizeof(value),"<b>%s</b>\n%s<b>%u%% · %u/%u/%u</b>%s\nCP %u · Size %s",e->name,perfect?"<color=#D52632>":"",((e->stats.attack+e->stats.defense+e->stats.stamina)*100+22)/45,e->stats.attack,e->stats.defense,e->stats.stamina,perfect?"</color>":"",e->cp,collection_size(e));collection_text(s,text,value);return view;
}
static void *collection_hint(Enhancer *s,void *panel){
    void *section=NULL,*transform=NULL,*child=NULL,*text=NULL;if(!object_field(s,panel,"eggSection","Niantic.Holoholo.UI.PokemonInfoPanelEggSection",&section)||!read_ref(s,section,"get_transform",&transform))return NULL;
    const char *str[]={"System.String"},*type_arg[]={"System.Type"};void *name=s->base.string_new_len("EggHatchLabel",13),*args[]={name};if(!name||!invoke(s,collection_method(s,s->base.object_get_class(transform),"Find","UnityEngine.Transform",1,str),transform,args,&child)||!child)return NULL;
    void *reflection=s->api.type_object(s->base.class_get_type(s->text_class)),*component_args[]={reflection};if(!reflection||!invoke(s,collection_method(s,s->base.object_get_class(child),"GetComponent","UnityEngine.Component",1,type_arg),child,component_args,&text))return NULL;return text;
}
static void collection_panel(void *o,void *pokemon,const void *m){
    Enhancer *s=owned();ENH_SCOPE(s);collection_panel_original(o,pokemon,m);if(!collection_enabled(s,1))return;collection_sources(s,o);CollectionEntry *e=collection_entry(s,pokemon);if(!e)return;
    void *name=NULL,*hint=collection_hint(s,o);if(object_field(s,o,"nameText","UnityEngine.UI.Text",&name))collection_text(s,name,e->name);
    char text[768];unsigned percent=((e->stats.attack+e->stats.defense+e->stats.stamina)*100+22)/45;
    bool perfect=percent==100;snprintf(text,sizeof(text),"<size=28><b>%s</b></size>\n<size=24><b>CP %u · %sIV %u%% · %u/%u/%u%s</b>\nSize %s%s%s%s%s</size>",e->name,e->cp,perfect?"<color=#D52632>":"",percent,e->stats.attack,e->stats.defense,e->stats.stamina,perfect?"</color>":"",collection_size(e),e->fast?"\nFast: ":"",e->fast?e->moves[0]:"",e->charged?" · Charged: ":"",e->charged?e->moves[1]:"");
    if(hint)collection_text(s,hint,text);else if(name){snprintf(text,sizeof(text),"%s\n<size=23>%s<b>IV %u%% · %u/%u/%u</b>%s\nCP %u · Size %s</size>",e->name,perfect?"<color=#D52632>":"",percent,e->stats.attack,e->stats.defense,e->stats.stamina,perfect?"</color>":"",e->cp,collection_size(e));collection_text(s,name,text);}
}
static uint32_t collection_reward_species(Enhancer *s,void *data,bool *hidden){
    if(!exact(s,data,s->collection.data))return 0;void *reward=NULL,*encounter=NULL;s->api.get(data,s->collection.data_reward,&reward);if(!reward||!read_ref(s,reward,"get_PokemonEncounter",&encounter))return 0;
    collection_bool(s,encounter,"IsHiddenDitto",hidden);if(*hidden)return 132;bool has=false;if(!scalar(s,collection_method(s,s->base.object_get_class(encounter),"get_HasPokemonId","System.Boolean",0,NULL),encounter,&has,1)||!has)return 0;
    int32_t id=0;if(!scalar(s,collection_method(s,s->base.object_get_class(encounter),"get_PokemonId","Holoholo.Rpc.HoloPokemonId",0,NULL),encounter,&id,4)||id<1||id>=2048)return 0;return(uint32_t)id;
}
static void collection_reward(void *o,void *data,const void *m){
    Enhancer *s=owned();ENH_SCOPE(s);bool hidden=false;uint32_t species=collection_enabled(s,2)?collection_reward_species(s,data,&hidden):0;int32_t display=0;bool show_name=false;
    void *previous=NULL;if(s&&object_field(s,o,"itemName","UnityEngine.UI.Text",&previous))collection_reward_position(s,previous,NULL);
    if(species){s->api.get(data,s->collection.data_display,&display);s->api.get(data,s->collection.data_name,&show_name);int32_t reveal=1;bool yes=true;if(!hidden)s->api.set(data,s->collection.data_display,&reveal);s->api.set(data,s->collection.data_name,&yes);}
    collection_reward_original(o,data,m);
    if(species){s->api.set(data,s->collection.data_display,&display);s->api.set(data,s->collection.data_name,&show_name);void *label=NULL;char name[112],text[192];collection_name(species,0,name,sizeof(name));snprintf(text,sizeof(text),"<color=#36595E><size=24><b>%s</b></size></color>",name);if(object_field(s,o,"itemName","UnityEngine.UI.Text",&label)){collection_text(s,label,text);collection_reward_position(s,label,o);}}
}
static void collection_open(Enhancer *s,SilphManagedResolver resolver,void *context){
    if(!s->config.collection_info||(s->config.flags&SILPH_ENH_AUTORAID))return;CollectionView *v=&s->collection;
    void *symbol=resolver("il2cpp_field_get_name",context);if(symbol)memcpy(&v->field_name,&symbol,sizeof(symbol));symbol=resolver("il2cpp_field_static_get_value",context);if(symbol)memcpy(&v->static_get,&symbol,sizeof(symbol));
    const char *clone_args[]={"UnityEngine.Object","UnityEngine.Transform","System.Boolean"},*destroy_args[]={"UnityEngine.Object"};void *unity_object=klass(s,"UnityEngine.CoreModule.dll","UnityEngine","Object");v->instantiate=collection_static(s,unity_object,"Instantiate","UnityEngine.Object",3,clone_args);v->destroy=collection_static(s,unity_object,"Destroy","System.Void",1,destroy_args);
    s->text_class=klass(s,"UnityEngine.UI.dll","UnityEngine.UI","Text");s->pokemon_class=klass(s,"holo-protos.dll","Holoholo.Rpc","PokemonProto");s->text_get=method(s,s->text_class,"get_text","System.String",0,NULL);const char *str[]={"System.String"},*bit[]={"System.Boolean"},*wrap[]={"UnityEngine.VerticalWrapMode"};s->text_set=method(s,s->text_class,"set_text","System.Void",1,str);s->rich_get=method(s,s->text_class,"get_supportRichText","System.Boolean",0,NULL);s->rich_set=method(s,s->text_class,"set_supportRichText","System.Void",1,bit);s->overflow_get=method(s,s->text_class,"get_verticalOverflow","UnityEngine.VerticalWrapMode",0,NULL);s->overflow_set=method(s,s->text_class,"set_verticalOverflow","System.Void",1,wrap);
    if(!s->text_get||!s->text_set||!s->rich_get||!s->rich_set||!s->overflow_get||!s->overflow_set){__android_log_print(ANDROID_LOG_INFO,"SilphCollection","collection text unavailable");return;}
    if(s->config.collection_info&1){v->adapter=klass(s,"holo-game.dll","Niantic.Holoholo.Inventory","EggListAdapter");v->view=klass(s,"holo-game.dll","Niantic.Holoholo.Inventory","EggListItemView");v->panel=klass(s,"holo-game.dll","Niantic.Holoholo.UI.Pokemon","PokemonInfoPanel");v->settings=klass(s,"holo-protos.dll","Holoholo.Rpc","PokemonSettingsProto");v->move_enum=klass(s,"holo-protos.dll","Holoholo.Rpc","HoloPokemonMove");
        const void *master=field(s,v->panel,"egmu","IGameMasterData");if(master)v->master_type=s->api.class_from_type(s->api.field_type(master));const char *args[]={"System.Int32","Holoholo.Rpc.PokemonProto"},*one[]={"Holoholo.Rpc.PokemonProto"},*level[]={"IGameMasterData","System.Int32"};s->level_at=static_method(s,klass(s,"holo-game-core.dll","Niantic.Holoholo","GameMasterDataExt_PlayerLevelSettings"),"CpMultiplierAtLevel","System.Single",2,level);
        v->egg_create=collection_method(s,v->adapter,"CreateView","Niantic.Holoholo.Inventory.EggListItemView",2,args);v->panel_update=collection_method(s,v->panel,"DoUpdate","System.Void",1,one);
        if(v->master_type&&v->settings&&s->level_at&&v->egg_create&&v->panel_update&&!hook(s,v->egg_create,(void *)collection_create,(void **)&collection_create_original)&&!hook(s,v->panel_update,(void *)collection_panel,(void **)&collection_panel_original))v->eggs=1;
    }
    if(s->config.collection_info&2){v->reward=klass(s,"holo-game.dll","Niantic.Holoholo.Quests","QuestRewardItem");void *it=NULL;for(unsigned i=0;v->reward&&i<32;i++){void *n=s->api.nested(v->reward,&it);if(!n)break;const char *name=s->api.class_name(n);if(name&&!strcmp(name,"Data")){v->data=n;break;}}
        if(v->data&&!s->api.is_value(v->data)){v->data_reward=field(s,v->data,"Reward","Holoholo.Rpc.QuestRewardProto");v->data_display=enum_field(s,v->data,"EncounterDisplay");v->data_name=field(s,v->data,"ShowItemName","System.Boolean");const char *arg[]={"Niantic.Holoholo.Quests.QuestRewardItem.Data"};v->reward_update=collection_method(s,v->reward,"DoUpdate","System.Void",1,arg);if(v->data_reward&&v->data_display&&v->data_name&&v->reward_update&&!hook(s,v->reward_update,(void *)collection_reward,(void **)&collection_reward_original))v->rewards=1;}
    }
    __android_log_print(ANDROID_LOG_INFO,"SilphCollection","collection prepared eggs=%u research=%u hooks=%u",v->eggs,v->rewards,s->used);
}
static void collection_close(Enhancer *s){for(unsigned i=0;i<32;i++)collection_badge_destroy(s,&s->collection.badges[i]);for(unsigned i=0;i<s->collection.label_count;i++)collection_restore(s,&s->collection.labels[i]);drop(s,&s->collection.master);drop(s,&s->collection.player);}
