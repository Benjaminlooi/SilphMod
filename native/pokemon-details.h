#ifndef SILPH_POKEMON_DETAILS_H
#define SILPH_POKEMON_DETAILS_H
#include <stdint.h>

typedef struct {
    uint32_t species,form,cp,ivs,shiny;
} SilphPokemonDetails;

enum {SILPH_FARMER_CAUGHT=1,SILPH_FARMER_KEPT,SILPH_FARMER_TRANSFERRED};

/* Zero denotes missing data, so 0/0/0 is encoded as one. */
static inline uint32_t silph_pack_ivs(uint32_t attack,uint32_t defense,uint32_t stamina){
    return attack<=15&&defense<=15&&stamina<=15?1+(attack<<8)+(defense<<4)+stamina:0;
}
#endif
