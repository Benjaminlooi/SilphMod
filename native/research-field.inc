static int field_value(Research *s,SilphManagedRef ref,const char *name,uint32_t kind,uint64_t *out){SilphReadValue v={0};int r=silph_read_field(s->read,ref,name,kind,&v);if(!r)*out=v.bits;return r;}
static int field_get(Research *s,SilphManagedRef ref,const char *name,uint32_t kind,SilphReadValue *out){return silph_read_call(s->read,ref,name,kind,NULL,0,out);}
static int field_ref(Research *s,SilphManagedRef ref,const char *name,SilphManagedRef *out){SilphReadValue v={0};int r=silph_read_field(s->read,ref,name,SILPH_READ_REFERENCE,&v);if(!r)*out=v.reference;return r;}
static void field_describe(Research *s,SilphManagedRef quest,SilphFieldTask *out){
    *out=(SilphFieldTask){0};uint64_t value=0;SilphManagedRef goal={0},conditions={0},condition={0},rewards={0},reward={0};SilphReadValue detail={0},v={0},fort={0};uint32_t n=0;
    int error=integer(s,quest,"QuestContext",&out->context);if(!error)error=integer(s,quest,"QuestType",&out->type);if(!error)error=integer(s,quest,"Status",&out->status);
    if(!error)error=field_value(s,quest,"IsBonusChallenge",SILPH_READ_BOOL,&value);out->bonus=(uint32_t)value;
    if(!error)error=silph_read_field(s->read,quest,"FortId",SILPH_READ_REFERENCE,&fort);
    if(!error&&fort.reference.serial){char id[257]={0};uint32_t length=0;error=silph_read_utf8(s->read,fort.reference,id,sizeof(id),&length);out->from_stop=length>0;}
    if(!error)error=field_ref(s,quest,"Goal",&goal);if(!error&&!goal.serial)error=SILPH_MANAGED_NOT_READY;
    if(!error)error=field_value(s,goal,"Target",SILPH_READ_I32,&value);if(!error&&value>100000)error=SILPH_MANAGED_CONTRACT;if(!error)out->target=(uint32_t)value;
    if(!error)error=field_ref(s,goal,"Condition",&conditions);if(!error)error=count(s,conditions,32,&n);if(!error)out->conditions=n;
    for(uint32_t i=0;!error&&i<n;i++){
        uint32_t type=0;error=item(s,conditions,i,"QuestConditionProto",&condition);if(!error)error=integer(s,condition,"Type",&type);
        if(!error&&type==8){error=field_get(s,condition,"get_WithThrowType",SILPH_READ_REFERENCE,&detail);if(!error)error=field_get(s,detail.reference,"get_HasThrowType",SILPH_READ_BOOL,&v);if(!error&&v.bits){error=field_get(s,detail.reference,"get_ThrowType",SILPH_READ_ENUM32,&v);if(!error)out->throw_type=(uint32_t)v.bits;}}
        int a=drop(s,&detail.reference),b=drop(s,&condition);if(!error)error=a?a:b;
    }
    out->goal_known=!error;
    if(!error)error=field_ref(s,quest,"QuestRewards",&rewards);if(!error)error=count(s,rewards,32,&n);if(!error&&!n)error=SILPH_MANAGED_NOT_READY;
    for(uint32_t i=0;!error&&i<n;i++){
        uint32_t type=0;error=item(s,rewards,i,"QuestRewardProto",&reward);if(!error)error=integer(s,reward,"Type",&type);out->reward_type=type;
        if(!error&&(type==2||type==4)){
            error=field_get(s,reward,type==2?"get_Item":"get_Candy",SILPH_READ_REFERENCE,&detail);uint64_t id=0,amount=0;
            if(!error)error=field_value(s,detail.reference,type==2?"Item":"PokemonId",SILPH_READ_ENUM32,&id);
            if(!error)error=field_value(s,detail.reference,"Amount",SILPH_READ_I32,&amount);if(!error&&(amount>1000000||!amount))error=SILPH_MANAGED_CONTRACT;
            if(!error){out->reward_id=(uint32_t)id;out->reward_amount=(uint32_t)amount;}
            if(!error&&type==4&&id==890&&amount==25)out->reward_mask|=1;
            if(!error&&type==2&&id==708)out->reward_mask|=2;
        }else if(!error&&(type<1||type>20||type==18))error=SILPH_MANAGED_NOT_READY;
        int a=drop(s,&detail.reference),b=drop(s,&reward);if(!error)error=a?a:b;
    }
    out->rewards_known=!error;SilphManagedRef *refs[]={&fort.reference,&goal,&conditions,&condition,&rewards,&reward,&detail.reference};for(unsigned i=0;i<7;i++)drop(s,refs[i]);
}
static void field_log(Research *s,const char *id,const SilphFieldTask *t){
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_INFO,"SilphResearch","field quest=%s context=%u type=%u status=%u target=%u conditions=%u throw=%u bonus=%u fort=%u goalKnown=%u rewardsKnown=%u rewards=%u rewardType=%u rewardId=%u amount=%u filter=%u",id,t->context,t->type,t->status,t->target,t->conditions,t->throw_type,t->bonus,t->from_stop,t->goal_known,t->rewards_known,t->reward_mask,t->reward_type,t->reward_id,t->reward_amount,s->field_filter);
#else
    (void)s;(void)id;(void)t;
#endif
}
static void field_cached(Research *s,SilphManagedRef quest,SilphFieldTask *out){
    char id[257]={0};uint64_t created=0;int r=text(s,quest,"QuestId",id);if(!r)r=field_value(s,quest,"CreationTimestampMs",SILPH_READ_I64,&created);
    if(r){*out=(SilphFieldTask){0};return;}
    for(unsigned i=0;i<16;i++)if(s->fields[i].created==created&&!strcmp(s->fields[i].id,id)&&s->fields[i].task.goal_known&&s->fields[i].task.rewards_known&&(s->fields[i].task.from_stop||s->fields[i].task.bonus||s->fields[i].task.type==46)){*out=s->fields[i].task;return;}
    FieldEntry *entry=&s->fields[s->field_cursor++%16];memset(entry,0,sizeof(*entry));memcpy(entry->id,id,sizeof(entry->id));entry->created=created;field_describe(s,quest,&entry->task);*out=entry->task;field_log(s,id,out);
}
int silph_research_inspect(void *read,void *managed,void *game){
    Research *s=calloc(1,sizeof(Research));if(!s)return SILPH_MANAGED_CAPACITY;s->read=read;s->managed=managed;s->game=game;SilphManagedRef quests={0},list={0},quest={0};SilphReadValue proto={0};uint32_t n=0;
    int r=silph_game_read_research_service(game,0,&quests);if(!r)r=field_get(s,quests,"get_QuestsProto",SILPH_READ_REFERENCE,&proto);if(!r)r=field_ref(s,proto.reference,"Quest",&list);if(!r)r=count(s,list,512,&n);
    for(uint32_t i=0;!r&&i<n;i++){
        r=item(s,list,i,"QuestProto",&quest);uint32_t context=0;if(!r)r=integer(s,quest,"QuestContext",&context);
        if(!r&&context==2){SilphFieldTask t={0};field_cached(s,quest,&t);}int d=drop(s,&quest);if(!r)r=d;
    }
    SilphManagedRef *refs[]={&quest,&list,&proto.reference,&quests};for(unsigned i=0;i<4;i++){int d=drop(s,refs[i]);if(!r)r=d;}free(s);return r;
}
