#ifndef SILPH_RAID_ENTRY_POLICY_H
#define SILPH_RAID_ENTRY_POLICY_H

#include <stdint.h>
#include <stdbool.h>

enum {
    SILPH_RAID_ENTRY_WAIT = 0, SILPH_RAID_ENTRY_DETAILS = 1, SILPH_RAID_ENTRY_AWARD_FREE = 2,
    SILPH_RAID_ENTRY_FREE = 3, SILPH_RAID_ENTRY_PREMIUM = 4, SILPH_RAID_ENTRY_REJOIN = 5,
    SILPH_RAID_ENTRY_NO_PASSES = 6, SILPH_RAID_ENTRY_REAPPROACH = 7, SILPH_RAID_ENTRY_BLOCKED = 8
};
typedef struct {
    uint64_t candidate, details_candidate, raid_seed, details_at_ms, now_ms, inventory_revision, award_inventory_revision;
    int32_t free_count, premium_count, award_result;
    bool details_ready, in_range, can_join, paid_rejoin, free_eligible, inventory_ready, allow_premium, request_pending, award_sent;
} SilphRaidEntryInput;
typedef struct {
    uint32_t action, reason;
} SilphRaidEntryChoice;

int silph_raid_entry_decide(const SilphRaidEntryInput *, SilphRaidEntryChoice *);

#endif
