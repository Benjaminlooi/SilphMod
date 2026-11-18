#ifndef SILPH_RAID_POLICY_H
#define SILPH_RAID_POLICY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

enum {
    SILPH_RAID_WAIT = 0, SILPH_RAID_FAST = 1, SILPH_RAID_CHARGED = 2, SILPH_RAID_DODGE = 3
};
typedef struct {
    uint32_t slot, move, duration_ms;
    int32_t energy_delta;
    double power, effectiveness, damage_scale;
} SilphRaidMove;
typedef struct {
    uint32_t action, slot, move, duration_ms;
} SilphRaidMoveChoice;

int silph_raid_choose_move(const SilphRaidMove *, size_t, int32_t, bool, uint32_t,
                           SilphRaidMoveChoice *);

bool silph_raid_lobby_leave(int64_t, uint32_t, uint32_t, uint32_t, bool);

uint32_t silph_raid_reward_ball(uint32_t, uint32_t);
uint32_t silph_raid_stage_wait_ms(uint32_t);

uint32_t silph_raid_modal_answer(const char *, bool, uint32_t, bool, bool);

bool silph_raid_target_ready(uint32_t, uint32_t, uint64_t, uint64_t);
enum {
    SILPH_RAID_CATCH_HUNDO = 1,
    SILPH_RAID_CATCH_BACKGROUND = 2,
    SILPH_RAID_CATCH_SHINY = 4,
    SILPH_RAID_CATCH_SHINY_HUNDO = 8,
    SILPH_RAID_CATCH_SHINY_BACKGROUND = 16
};
enum {
    SILPH_RAID_KNOWN_IV = 1, SILPH_RAID_KNOWN_SHINY = 2, SILPH_RAID_KNOWN_BACKGROUND = 4
};

int silph_raid_catch_accept(uint32_t, uint32_t, int32_t, int32_t, int32_t, bool, bool);

#endif
