#include "event-actions.h"
#include <string.h>
#include <android/log.h>
static const uint32_t species[]={103,212,330,365,376,395,462,534,612,184,230,306,598,635,706,715,959,980,380,381,146,145,144};
int silph_event_accept(uint32_t filters,uint32_t a,uint32_t d,uint32_t h,uint32_t shiny){
    if(filters>SILPH_EVENT_CATCH_ALL||a>15||d>15||h>15||shiny>1)return -1;
    return filters==SILPH_EVENT_CATCH_ALL||!!(((filters&1)&&shiny)||((filters&2)&&a==15&&d==15&&h==15));
}
int silph_event_decode(const uint8_t *p,SilphEventPolicy *out){
    if(!p||!out)return SILPH_MANAGED_ARGUMENT;memset(out,0,sizeof(*out));
    if(p[0]!=0xe7||p[1]!=1||p[2]>SILPH_EVENT_CATCH_ALL||p[3]||p[8]<1||p[8]>4||p[13]>5)return SILPH_MANAGED_CONTRACT;
    out->enabled=1;out->filters=p[2];out->selected=(uint32_t)p[4]|(uint32_t)p[5]<<8|(uint32_t)p[6]<<16|(uint32_t)p[7]<<24;
    if(out->selected&~SILPH_EVENT_ALL)return SILPH_MANAGED_CONTRACT;out->ball_count=p[8];out->berry_count=p[13];
    for(unsigned i=0;i<4;i++){uint32_t id=p[9+i];if(i>=out->ball_count){if(id)return SILPH_MANAGED_CONTRACT;continue;}if(id!=1&&id!=2&&id!=3&&id!=7)return SILPH_MANAGED_CONTRACT;for(unsigned j=0;j<i;j++)if(id==out->balls[j])return SILPH_MANAGED_CONTRACT;out->balls[i]=id;}
    const uint32_t berries[]={701,703,705,706,708};
    for(unsigned i=0;i<5;i++){uint32_t code=p[14+i];if(i>=out->berry_count){if(code)return SILPH_MANAGED_CONTRACT;continue;}if(code<1||code>5)return SILPH_MANAGED_CONTRACT;out->berries[i]=berries[code-1];for(unsigned j=0;j<i;j++)if(out->berries[j]==out->berries[i])return SILPH_MANAGED_CONTRACT;}
    for(unsigned i=19;i<SILPH_EVENT_BYTES;i++)if(p[i])return SILPH_MANAGED_CONTRACT;return 0;
}
int silph_event_classify(void *read,void *managed,SilphManagedRef target,uint32_t id,int *slot){
    if(!slot)return SILPH_MANAGED_ARGUMENT;*slot=0;bool listed=false;for(unsigned i=0;i<23;i++)if(species[i]==id)listed=true;if(!listed)return 0;
    SilphReadValue display={0},strong={0},form={0};int r=silph_read_call(read,target,"get_PokemonDisplay",SILPH_READ_REFERENCE,NULL,0,&display);
    if(!r)r=silph_read_is_type(read,display.reference,"Holoholo.Rpc","PokemonDisplayProto");
    if(!r)r=silph_read_field(read,display.reference,"IsStrongPokemon",SILPH_READ_BOOL,&strong);
    if(!r)r=silph_read_field(read,display.reference,"Form",SILPH_READ_ENUM32,&form);
    if(!r)for(unsigned i=0;i<23;i++)if(species[i]==id){bool regional=id==103?form.bits==78:id==144?form.bits==2801:id==145?form.bits==2800:id==146?form.bits==2799:true;if(regional&&(strong.bits||i>=18))*slot=(int)i+1;break;}
    int d=display.reference.serial?silph_managed_release(managed,display.reference):0;return r?r:d;
}
static int encounter_pokemon(void *read,void *managed,SilphManagedRef target,bool quest,SilphManagedRef *out,int *source){
    SilphReadValue pokemon={0},value={0};SilphManagedRef response={0},wild={0};
    const char *types[]={"EncounterOutProto","IncenseEncounterOutProto","DiskEncounterOutProto","QuestEncounterOutProto"};int kind=-1,r=SILPH_MANAGED_NOT_READY;
    for(unsigned i=quest?3:0;i<(quest?4u:3u);i++){int found=silph_read_field_type(read,target,"Holoholo.Rpc",types[i],&response);if(!found&&response.serial){kind=(int)i;r=0;break;}if(response.serial){silph_managed_release(managed,response);response=(SilphManagedRef){0};}}
    if(!r)r=silph_read_field(read,response,kind==0?"Status":"Result",SILPH_READ_ENUM32,&value);if(!r&&value.bits!=1)r=SILPH_MANAGED_NOT_READY;
    if(!r&&kind==0){r=silph_read_field(read,response,"Pokemon",SILPH_READ_REFERENCE,&value);wild=value.reference;if(!r)r=silph_read_is_type(read,wild,"Holoholo.Rpc","WildPokemonProto");if(!r)r=silph_read_field(read,wild,"EncounterId",SILPH_READ_U64,&value);uint64_t id=value.bits;if(!r)r=silph_read_call(read,target,"get_EncounterId",SILPH_READ_U64,NULL,0,&value);if(!r&&value.bits!=id)r=SILPH_MANAGED_STALE;}
    if(!r)r=silph_read_field(read,kind==0?wild:response,"Pokemon",SILPH_READ_REFERENCE,&pokemon);
    if(!r)r=silph_read_is_type(read,pokemon.reference,"Holoholo.Rpc","PokemonProto");
    if(wild.serial){int d=silph_managed_release(managed,wild);if(!r)r=d;}if(response.serial){int d=silph_managed_release(managed,response);if(!r)r=d;}
    if(r&&pokemon.reference.serial)silph_managed_release(managed,pokemon.reference);
    if(!r){*out=pokemon.reference;if(source)*source=kind;}
    return r;
}
static int read_ivs(void *read,SilphManagedRef pokemon,uint32_t iv[3]){
    const char *fields[]={"individualAttack_","individualDefense_","individualStamina_"};
    for(unsigned i=0;i<3;i++){
        SilphReadValue value;
        int status=silph_read_field(read,pokemon,fields[i],SILPH_READ_I32,&value);
        if(status)return status;
        iv[i]=(uint32_t)value.bits;
    }
    return 0;
}
void silph_farmer_pokemon_details(void *read,void *managed,SilphManagedRef pokemon,SilphPokemonDetails *details){
    SilphReadValue value,display={0};
    uint32_t iv[3];
    if(!read_ivs(read,pokemon,iv))details->ivs=silph_pack_ivs(iv[0],iv[1],iv[2]);
    if(!silph_read_field(read,pokemon,"cp_",SILPH_READ_I32,&value)&&value.bits<=INT32_MAX)
        details->cp=(uint32_t)value.bits;
    if(!silph_read_field(read,pokemon,"pokemonDisplay_",SILPH_READ_REFERENCE,&display)
        &&!silph_read_is_type(read,display.reference,"Holoholo.Rpc","PokemonDisplayProto")){
        if(!silph_read_field(read,display.reference,"Form",SILPH_READ_ENUM32,&value)&&value.bits<=INT32_MAX)
            details->form=(uint32_t)value.bits;
        if(!silph_read_field(read,display.reference,"Shiny",SILPH_READ_BOOL,&value))details->shiny=value.bits==1;
    }
    if(display.reference.serial)silph_managed_release(managed,display.reference);
}
void silph_farmer_encounter_details(void *read,void *managed,SilphManagedRef target,bool quest,SilphPokemonDetails *details){
    SilphManagedRef pokemon={0};
    if(!encounter_pokemon(read,managed,target,quest,&pokemon,NULL)){
        silph_farmer_pokemon_details(read,managed,pokemon,details);
        silph_managed_release(managed,pokemon);
    }
}
int silph_event_filter(void *read,void *managed,SilphManagedRef target,const SilphEventPolicy *policy,bool *accepted){
    if(!accepted||!policy)return SILPH_MANAGED_ARGUMENT;*accepted=false;
    SilphReadValue display={0},value={0};SilphManagedRef pokemon={0};uint32_t iv[3]={0},shiny=0;int kind=-1;
    int r=encounter_pokemon(read,managed,target,false,&pokemon,&kind);
    if(!r)r=read_ivs(read,pokemon,iv);
    if(!r)r=silph_read_field(read,pokemon,"pokemonDisplay_",SILPH_READ_REFERENCE,&display);
    if(!r)r=silph_read_field(read,display.reference,"Shiny",SILPH_READ_BOOL,&value);shiny=(uint32_t)value.bits;
    int accept=silph_event_accept(policy->filters,iv[0],iv[1],iv[2],shiny);if(!r&&accept<0)r=SILPH_MANAGED_CONTRACT;if(!r)*accepted=accept!=0;
    if(!r)__android_log_print(ANDROID_LOG_INFO,"SilphEvent","verified iv=%u/%u/%u shiny=%u accepted=%u source=%d",iv[0],iv[1],iv[2],shiny,*accepted,kind);
    int a=display.reference.serial?silph_managed_release(managed,display.reference):0,b=pokemon.serial?silph_managed_release(managed,pokemon):0;return r?r:a?a:b;
}
