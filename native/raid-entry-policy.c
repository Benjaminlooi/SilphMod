#include "raid-entry-policy.h"

int silph_raid_entry_decide(const SilphRaidEntryInput *in, SilphRaidEntryChoice *out) {
    if (!in || !out)return 1;
    *out = (SilphRaidEntryChoice) {0};
    if (!in->candidate || !in->now_ms || in->free_count < 0 || in->premium_count < 0 ||
        in->award_result < 0 || in->award_result > 5)
        return 1;
    if (in->request_pending)return 0;
    if (!in->details_ready || in->details_candidate != in->candidate || !in->raid_seed ||
        in->details_at_ms > in->now_ms || in->now_ms - in->details_at_ms > 10000) {
        out->action = SILPH_RAID_ENTRY_DETAILS;
        return 0;
    }
    if (!in->in_range) {
        out->action = SILPH_RAID_ENTRY_REAPPROACH;
        return 0;
    }
    if (in->paid_rejoin) {
        out->action = SILPH_RAID_ENTRY_REJOIN;
        return 0;
    }
    if (!in->can_join) {
        out->action = SILPH_RAID_ENTRY_BLOCKED;
        return 0;
    }
    if (!in->inventory_ready)return 0;
    if (in->free_count) {
        out->action = SILPH_RAID_ENTRY_FREE;
        return 0;
    }
    if (in->award_sent) {
        if (!in->award_result)return 0;
        out->reason = (uint32_t) in->award_result;
        if (in->award_result == 1) {
            if (in->inventory_revision <= in->award_inventory_revision)return 0;
            out->action = SILPH_RAID_ENTRY_DETAILS;
            return 0;
        }
        if (in->award_result == 4) {
            out->action = SILPH_RAID_ENTRY_REAPPROACH;
            return 0;
        }
        if (in->award_result == 2) {
            out->action = SILPH_RAID_ENTRY_BLOCKED;
            return 0;
        }
    } else if (in->free_eligible) {
        out->action = SILPH_RAID_ENTRY_AWARD_FREE;
        return 0;
    }
    out->action = in->allow_premium && in->premium_count ? SILPH_RAID_ENTRY_PREMIUM
                                                         : SILPH_RAID_ENTRY_NO_PASSES;
    return 0;
}
