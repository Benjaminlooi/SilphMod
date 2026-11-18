#ifndef SILPH_RAID_READ_H
#define SILPH_RAID_READ_H

#include "managed-read.h"

typedef struct {
    char gym[161];
    double latitude, longitude;
    uint64_t seed, battle_ms, end_ms, server_ms;
    uint32_t level, species, form, costume, gender, evolution, evolution_known, complete, active, display_known;
    uint32_t verified, free_available, ticket_used, remote_used, players, state, pass_known, can_join, in_range, details_result;
    uint32_t full_details, rewards_claimed, flushed;
    int32_t throws_remaining;
} SilphRaidSite;

size_t silph_raid_read_size(void);

int silph_raid_read_open(void *, size_t, void *, void *, void *);

int silph_raid_read_begin(void *);

int silph_raid_read_end(void *);

int silph_raid_read_next(void *, SilphRaidSite *, bool *);

int silph_raid_read_current(void *, SilphRaidSite *);

int silph_raid_read_gym(void *, const char *, SilphManagedRef *);

int silph_raid_read_close(void *);

#endif
