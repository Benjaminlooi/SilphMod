#ifndef SILPH_RAID_CLOCK_POLICY_H
#define SILPH_RAID_CLOCK_POLICY_H

#include <stdint.h>

typedef struct {
    uint64_t server, at, last;
} SilphRaidClock;

static inline int64_t silph_raid_clock_now(SilphRaidClock *clock, uint64_t server, uint64_t now) {
    if (!clock)return -1;
    if (server >= UINT64_C(1577836800000) && server <= UINT64_C(4102444800000) &&
        server > clock->last) {
        clock->server = server;
        clock->at = now;
        clock->last = server;
    }
    if (!clock->server || now < clock->at)return -1;
    uint64_t elapsed = (now - clock->at) / UINT64_C(1000000);
    if (elapsed > UINT64_C(4102444800000) - clock->server)return -1;
    return (int64_t)(clock->server + elapsed);
}

#endif
