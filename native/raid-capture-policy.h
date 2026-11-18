#ifndef SILPH_RAID_CAPTURE_POLICY_H
#define SILPH_RAID_CAPTURE_POLICY_H

#include <stdbool.h>
#include <stdint.h>

static inline bool
silph_raid_capture_response_valid(int32_t status, int32_t remaining, uint32_t available_before,
                                  uint64_t captured) {
    return status >= 1 && status <= 4 && remaining >= 0 &&
           (uint32_t) remaining <= available_before && (status != 1 || captured != 0);
}

#endif
