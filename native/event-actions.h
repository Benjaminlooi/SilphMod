#ifndef SILPH_EVENT_ACTIONS_H
#define SILPH_EVENT_ACTIONS_H
#include "game-read.h"
#include "pokemon-details.h"
#define SILPH_EVENT_OFFSET 232u
#define SILPH_EVENT_BYTES 24u
#define SILPH_EVENT_CATCH_ALL 4u
#define SILPH_EVENT_ALL UINT32_C(8388607)
typedef struct {uint32_t enabled,filters,selected,ball_count,balls[4],berry_count,berries[5];} SilphEventPolicy;
int silph_event_decode(const uint8_t *,SilphEventPolicy *);
int silph_event_classify(void *,void *,SilphManagedRef,uint32_t,int *);
int silph_event_filter(void *,void *,SilphManagedRef,const SilphEventPolicy *,bool *);
int silph_event_accept(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
void silph_farmer_encounter_details(void *,void *,SilphManagedRef,bool,SilphPokemonDetails *);
void silph_farmer_pokemon_details(void *,void *,SilphManagedRef,SilphPokemonDetails *);
#endif
