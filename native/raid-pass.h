#ifndef SILPH_RAID_PASS_H
#define SILPH_RAID_PASS_H

#include "managed-promise.h"
#include "scheduler.h"

typedef struct {
    uint64_t requests, raid_seed;
    uint32_t pending, result, error, uncertain;
    char gym[161];
} SilphRaidPassMetrics;

int silph_raid_award_free(void *, const SilphSchedulePermit *, SilphManagedRef, SilphManagedRef,
                          uint64_t, SilphReadValue *, bool *);

size_t silph_raid_pass_size(void);

int silph_raid_pass_open(void *, size_t, void *, void *, SilphManagedRef,
                         const SilphScheduleIdentity *);

int silph_raid_pass_request(void *, const SilphSchedulePermit *, const char *, uint64_t, uint64_t);

int silph_raid_pass_poll(void *, uint64_t, SilphRaidPassMetrics *);

int silph_raid_pass_close(void *);

#endif
