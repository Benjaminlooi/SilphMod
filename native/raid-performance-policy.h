#ifndef SILPH_RAID_PERFORMANCE_POLICY_H
#define SILPH_RAID_PERFORMANCE_POLICY_H

#include <stdbool.h>
#include <stdint.h>

static inline bool silph_raid_map_accelerate(unsigned phase, bool surveying) {
    return phase == 3 || (phase == 1 && surveying);
}

static inline int32_t silph_raid_graphics_target(unsigned kind, int32_t current) {
    return kind == 3 ? (current > 60 ? 60 : current) : 0;
}

static inline float silph_raid_render_target(float current) {
    return current > .70f ? .70f : current;
}

static inline unsigned
silph_raid_restore_decision(bool restored, bool expired, unsigned attempts, uint64_t now,
                            uint64_t next) {
    if (restored)return 1;
    if (!expired || now < next)return 0;
    return attempts < 2 ? 2 : 3;
}

#endif
