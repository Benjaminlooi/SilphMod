#include "raid-policy.h"
#include <limits.h>
#include <string.h>

static bool valid(const SilphRaidMove *move) {
    return move->slot >= 1 && move->slot <= 3 && move->move && move->duration_ms &&
           move->duration_ms <= 60000
           && move->energy_delta >= -10000 && move->energy_delta <= 10000;
}

static double positive(double value, double fallback) {
    return __builtin_isfinite(value) && value > 0 && value < 1000000 ? value : fallback;
}

int silph_raid_choose_move(const SilphRaidMove *moves, size_t count, int32_t energy, bool dodge_due,
                           uint32_t available_ms, SilphRaidMoveChoice *out) {
    if (!out)return 1;
    *out = (SilphRaidMoveChoice) {0};
    if (count > 3 || (!moves && count) || energy < 0 || energy > 10000)return 1;
    if (dodge_due) {
        out->action = SILPH_RAID_DODGE;
        return 0;
    }
    const SilphRaidMove *fast = NULL, *best = NULL;
    double fast_rate = 0, best_score = 0;
    bool best_super = false;
    for (size_t i = 0; i < count; i++)
        if (valid(&moves[i]) && moves[i].energy_delta >= 0 && moves[i].slot == 1) {
            fast = &moves[i];
            fast_rate = (double) fast->energy_delta / fast->duration_ms;
            break;
        }
    for (size_t i = 0; i < count; i++) {
        const SilphRaidMove *move = &moves[i];
        if (!valid(move) || move->slot == 1 || move->energy_delta >= 0 ||
            -move->energy_delta > energy || move->duration_ms > available_ms)
            continue;
        double effect = positive(move->effectiveness, 1), power = positive(move->power,
                                                                           1), scale = positive(
                move->damage_scale, 1);
        double cycle =
                move->duration_ms + (fast_rate > 0 ? -move->energy_delta / fast_rate : 0), score =
                power * effect * scale / cycle;
        bool super = effect > 1.000001;
        if (!best || (super && !best_super) || (super == best_super && (score > best_score ||
                                                                        (score == best_score &&
                                                                         move->duration_ms <
                                                                         best->duration_ms)))) {
            best = move;
            best_score = score;
            best_super = super;
        }
    }
    if (best)
        *out = (SilphRaidMoveChoice) {SILPH_RAID_CHARGED, best->slot, best->move,
                                      best->duration_ms};
    else if (fast && fast->duration_ms <= available_ms)
        *out = (SilphRaidMoveChoice) {SILPH_RAID_FAST, fast->slot, fast->move, fast->duration_ms};
    return 0;
}

bool
silph_raid_target_ready(uint32_t selected, uint32_t country, uint64_t now, uint64_t not_before) {
    return selected <= 3 && country >= 1 && country <= 2 && (selected & (1u << (country - 1))) &&
           now >= not_before;
}

int silph_raid_catch_accept(uint32_t filters, uint32_t known, int32_t attack, int32_t defense,
                            int32_t stamina, bool shiny, bool background) {
    if (filters > 31 || known > 7)return -2;
    if (!filters)return 1;
    if ((known & SILPH_RAID_KNOWN_IV) &&
        (attack < 0 || attack > 15 || defense < 0 || defense > 15 || stamina < 0 || stamina > 15))
        return -2;
    bool hundo = attack == 15 && defense == 15 && stamina == 15, pending = false;
    const uint32_t bits[] = {1, 2, 4, 8, 16}, needs[] = {1, 4, 2, 3, 6};
    const bool accepted[] = {hundo, background, shiny, shiny && hundo, shiny && background};
    for (unsigned i = 0; i < 5; i++)
        if (filters & bits[i]) {
            if ((known & needs[i]) != needs[i])pending = true; else if (accepted[i])return 1;
        }
    return pending ? -1 : 0;
}

bool silph_raid_lobby_leave(int64_t remaining_ms, uint32_t threshold, uint32_t players,
                            uint32_t required, bool team_ready) {
    if (remaining_ms < 0 || remaining_ms > 600000 || threshold < 5 || threshold > 110 ||
        !required || required > 20 || players > 1000)
        return false;
    return (players < required && remaining_ms <= (int64_t) threshold * 1000) ||
           (!team_ready && remaining_ms <= 3000);
}

uint32_t silph_raid_reward_ball(uint32_t level, uint32_t item) {
    if (item >= 5 && item <= 8)return item;
    if (!item &&
        ((level >= 1 && level <= 7) || level == 9 || level == 10 || (level >= 11 && level <= 17)))
        return 5;
    return 0;
}

uint32_t silph_raid_stage_wait_ms(uint32_t stage) {
    switch (stage) {
        case 1:
        case 2:
            return 30000;
        case 3:
        case 6:
            return 180000;
        case 4:
            return 600000;
        case 7:
        case 9:
            return 90000;
        case 8:
            return 20000;
        default:
            return 0;
    }
}

uint32_t
silph_raid_modal_answer(const char *key, bool owned, uint32_t stage, bool ready, bool rejoining) {
    if (!key || !owned || !stage || stage > 9 || stage == 8)return 0;
    if (!strcmp(key, "weather_warning_title") || !strcmp(key, "tips_warning_modal_title") ||
        !strcmp(key, "party_play_expire_title"))
        return 1;
    if (!strcmp(key, "tutorial_need_mega_title") ||
        !strcmp(key, "raid_friend_requests_modal_title"))
        return 2;
    if (!strcmp(key, "ready_raid_ftue_title") && stage == 3 && ready)return 1;
    if (!strcmp(key, "rejoin_modal_title") && stage == 9 && rejoining)return 1;
    return 0;
}
