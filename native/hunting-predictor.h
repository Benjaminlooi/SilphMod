#ifndef SILPH_READER_PREDICTOR_H
#define SILPH_READER_PREDICTOR_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {uint32_t attack,defense,stamina,level;} SilphPrediction;
static inline uint32_t silph_predict_next(uint64_t *state,unsigned bits){*state=(*state*UINT64_C(25214903917)+11)&UINT64_C(281474976710655);return(uint32_t)(*state>>(48-bits));}
static inline uint32_t silph_predict_int(uint64_t *state,uint32_t bound){
    if(!(bound&(bound-1)))return(uint32_t)(((uint64_t)bound*silph_predict_next(state,31))>>31);
    for(;;){uint32_t n=silph_predict_next(state,31),r=n%bound;if((int32_t)(n-r+bound-1)>=0)return r;}
}
static inline SilphPrediction silph_predict(uint64_t id,bool boosted,bool strong){
    uint64_t state=(id^UINT64_C(27135163855739)^UINT64_C(25214903917))&UINT64_C(281474976710655);
    silph_predict_next(&state,1);silph_predict_next(&state,1);silph_predict_next(&state,24);
    uint32_t bound=boosted?12:16,minimum=boosted?4:0;
    if(strong){silph_predict_next(&state,1);silph_predict_next(&state,1);bound=3;minimum=13;}
    SilphPrediction p;p.attack=silph_predict_int(&state,bound)+minimum;p.defense=silph_predict_int(&state,bound)+minimum;p.stamina=silph_predict_int(&state,bound)+minimum;
    p.level=strong?silph_predict_int(&state,5)+(boosted?41:36):silph_predict_int(&state,30)+1+(boosted?5:0);return p;
}
#endif
