#ifndef SILPH_FARMER_VIEW_H
#define SILPH_FARMER_VIEW_H

#include "visual-tools.h"

#if defined(SILPH_PILOT_TRANSFER) && defined(SILPH_PRESENT_SCOPED)
#define SILPH_FARMER_MAP_GESTURE 1
#endif
enum {
    SILPH_VIEW_BLOCKED = 0,
    SILPH_VIEW_MAP = 1,
    SILPH_VIEW_POKEMON = 2,
    SILPH_VIEW_ITEMS = 3,
    SILPH_VIEW_PROFILE = 4,
    SILPH_VIEW_POKEMON_INFO = 5,
    SILPH_VIEW_STORE = 6,
    SILPH_VIEW_POKEDEX = 7
};
typedef struct {
    uint32_t available, kind, stage, hooks, remaining;
    uint64_t updates, pumps, menu_pumps, skipped, max_ns, created, released;
    char name[64];
#ifdef SILPH_FARMER_MAP_GESTURE
    uint64_t taps;double latitude,longitude;uint32_t tap_available,tap_error;
#endif
} SilphFarmerViewMetrics;

typedef void (*SilphFarmerPump)(void *, bool, uint32_t);

size_t silph_farmer_view_size(void);

int silph_farmer_view_open(void *, size_t, const SilphManagedApi *, SilphManagedResolver, void *,
                           uint32_t, void *, SilphVisualAllowed, void *, SilphVisualAddress,
                           SilphFarmerPump);

bool silph_farmer_view_ready(void *);

#ifdef SILPH_PILOT_ENHANCER
bool silph_farmer_view_map_interactive(void *,void *);
#endif

void silph_farmer_view_interval(void *, uint32_t);

void silph_farmer_view_continuation(void *, bool);

void silph_farmer_view_rocket(void *, bool);

void silph_farmer_view_kecleon(void *, bool);

#ifdef SILPH_FARMER_MAP_GESTURE
int silph_farmer_view_teleport(void *,bool);
#endif

int silph_farmer_view_metrics(void *, SilphFarmerViewMetrics *);

int silph_farmer_view_close(void *, SilphFarmerViewMetrics *);

#endif
