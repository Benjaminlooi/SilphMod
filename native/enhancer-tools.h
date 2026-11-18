#ifndef SILPH_ENHANCER_TOOLS_H
#define SILPH_ENHANCER_TOOLS_H

#include "visual-tools.h"
#include "session-wire.h"
#include "game-read.h"
#include "scheduler.h"

typedef struct {
    uint64_t transferred, encounters, catches, fled, throws, berries, encounter, captured, jumps, shinies, xp, stardust;
    double latitude, longitude;
    uint32_t available, failed, error, pending, reward_error, capture_notice;
    uint64_t raids, raid_request;
    uint32_t raid_phase, raid_stage, raid_reason;
} SilphEnhancerMetrics;
void silph_enhancer_shadow_sent(uint64_t, uint64_t);

void silph_enhancer_shadow_result(uint64_t, uint64_t, bool, uint64_t);

size_t silph_enhancer_size(void);

int
silph_enhancer_open(void *, size_t, const SilphManagedApi *, SilphManagedResolver, void *, uint32_t,
                    const SilphSessionConfig *, SilphVisualAllowed, void *, SilphVisualAddress);

int silph_enhancer_tick(void *, void *, void *, void *, const SilphSchedulePermit *, bool, bool,
                        uint64_t, SilphEnhancerMetrics *);

bool silph_enhancer_busy(void *);

int silph_enhancer_close(void *, SilphManagedMetrics *);

int
silph_enhancer_route_bind(void *, void *, void *, void *, void *, const SilphScheduleIdentity *);

int silph_enhancer_route_publish(void *);

#endif
