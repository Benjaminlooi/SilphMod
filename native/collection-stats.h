#ifndef SILPH_COLLECTION_STATS_H
#define SILPH_COLLECTION_STATS_H
#include "hunting-predictor.h"
#include <math.h>

typedef struct {uint32_t attack,defense,stamina,size,fast,charged,moves;double noise;} SilphCollectionStats;
static inline double silph_collection_double(uint64_t *state){uint64_t high=silph_predict_next(state,26),low=silph_predict_next(state,27);return(double)((high<<27)|low)/9007199254740992.;}
static inline bool silph_collection_stats(uint64_t id,uint32_t fast,uint32_t charged,SilphCollectionStats *out){
    if(!out||fast>256||charged>256)return false;*out=(SilphCollectionStats){0};
    uint64_t state=(id^UINT64_C(27135163855739)^UINT64_C(25214903917))&UINT64_C(281474976710655);
    silph_predict_next(&state,1);silph_predict_next(&state,1);silph_predict_next(&state,24);
    out->attack=silph_predict_int(&state,6)+10;out->defense=silph_predict_int(&state,6)+10;out->stamina=silph_predict_int(&state,6)+10;silph_predict_int(&state,30);
    bool ready=false;for(unsigned i=0;i<256;i++){double x=2*silph_collection_double(&state)-1,y=2*silph_collection_double(&state)-1,r=x*x+y*y;if(r>=1||r==0)continue;out->noise=y*sqrt(-2*log(r)/r);ready=true;break;}
    if(!ready)return false;double size=silph_collection_double(&state);out->size=size<.004?1:size<.029?2:size>=.996?4:size>=.971?3:0;
    if(fast&&charged){out->fast=silph_predict_int(&state,fast);out->charged=silph_predict_int(&state,charged);out->moves=1;}return true;
}
static inline uint32_t silph_collection_cp(uint32_t attack,uint32_t defense,uint32_t stamina,const SilphCollectionStats *stats,double multiplier){
    if(!stats||!attack||!defense||!stamina||attack>10000||defense>10000||stamina>10000||!isfinite(multiplier)||multiplier<=0||multiplier>1)return 0;
    double cp=floor((attack+stats->attack)*sqrt(defense+stats->defense)*sqrt(stamina+stats->stamina)*multiplier*multiplier/10.);return cp<10?10:cp>100000?0:(uint32_t)cp;
}
#endif
