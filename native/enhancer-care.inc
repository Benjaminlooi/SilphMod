static void care_track(Enhancer *s, void *actor) {
    if (!(s->config.flags & SILPH_ENH_RAID_HEAL) || !actor)return;
    void *roster = NULL, *iterator = NULL;
    if (!read_ref(s, actor, "get_RosterList", &roster) ||
        !read_ref(s, roster, "GetEnumerator", &iterator))
        return;
    void *k = s->base.object_get_class(iterator);
    const void *next = method(s, k, "MoveNext", "System.Boolean", 0, NULL), *get = method(s, k,
                                                                                          "get_Current",
                                                                                          NULL, 0,
                                                                                          NULL);
    for (unsigned i = 0; i < 18; i++) {
        bool more = false;
        void *member = NULL, *pokemon = NULL;
        uint64_t id = 0;
        if (!scalar(s, next, iterator, &more, 1) || !more)break;
        if (!invoke(s, get, iterator, NULL, &member) ||
            !read_ref(s, member, "get_Pokemon", &pokemon))
            continue;
        const void *f = field(s, s->base.object_get_class(pokemon), "Id", "System.UInt64");
        if (f)s->api.get(pokemon, f, &id);
        if (!id)continue;
        bool seen = false;
        for (unsigned j = 0; j < s->care_count; j++)if (s->care_team[j] == id)seen = true;
        if (!seen && s->care_count < 18)s->care_team[s->care_count++] = id;
    }
}

#ifdef SILPH_PILOT_AUTORAID
#include "autoraid-care.inc"
#endif

static void
care_tick(Enhancer *s, void *read, void *game, void *managed, const SilphSchedulePermit *permit,
          uint64_t now, bool safe) {
#ifdef SILPH_PILOT_AUTORAID
    if((s->config.flags&SILPH_ENH_AUTORAID)&&(s->config.flags&SILPH_ENH_RAID_HEAL)&&ar_care_tick(s,read,game,managed,permit,now,safe))return;
#endif
    if (s->care_pending.serial) {
        void *value = NULL;
        bool pending = false;
        bool ok = promise_value(s, &s->care_pending, s->pokemon_class, &value, &pending);
        if (pending && now - s->care_at < UINT64_C(15000000000))return;
        int32_t hp = 0;
        uint64_t id = 0;
        const void *f = value ? field(s, s->pokemon_class, "id_", "System.UInt64") : NULL;
        if (f)s->api.get(value, f, &id);
        if (!pending && ok && id == s->care_target &&
            scalar(s, method(s, s->pokemon_class, "get_Stamina", "System.Int32", 0, NULL), value,
                   &hp, 4) && hp > s->care_hp) {
            s->care_expected = hp;
            s->care_sync_until = now + UINT64_C(8000000000);
            __android_log_print(ANDROID_LOG_INFO, "SilphEnhancer",
                                "raid team medicine confirmed hp=%d", hp);
        }
        else {
            s->care_index++;
            s->care_expected = 0;
            __android_log_print(ANDROID_LOG_INFO, "SilphEnhancer",
                                "raid team medicine unconfirmed; no resend");
        }
        drop(s, &s->care_pending);
        s->care_next = now + UINT64_C(500000000);
        return;
    }
    if (!safe || !s->care_ready || now < s->care_next ||
        !(s->config.flags & SILPH_ENH_RAID_HEAL))
        return;
    if (s->care_index >= s->care_count) {
        if (s->care_count)
            __android_log_print(ANDROID_LOG_INFO, "SilphEnhancer",
                                "raid team care completed members=%u", s->care_count);
        s->care_count = s->care_index = s->care_ready = 0;
        return;
    }
    uint64_t id = s->care_team[s->care_index];
    SilphManagedRef pokemon = {0};
    SilphReadValue hp = {0}, max = {0};
    int r = silph_game_read_owned(game, id, &pokemon);
    if (!r && pokemon.serial)
        r = silph_read_field(read, pokemon, "stamina_", SILPH_READ_I32, &hp);
    else r = SILPH_MANAGED_NOT_READY;
    if (!r)r = silph_read_field(read, pokemon, "maxStamina_", SILPH_READ_I32, &max);
    if (pokemon.serial)silph_managed_release(managed, pokemon);
    s->care_next = now + UINT64_C(500000000);
    if (r || max.bits == 0 || hp.bits >= max.bits) {
        s->care_index++;
        s->care_expected = 0;
        return;
    }
    if (s->care_expected) {
        if (hp.bits < (uint64_t) s->care_expected) {
            if (now >= s->care_sync_until) {
                s->care_index++;
                s->care_expected = 0;
            }
            return;
        }
        s->care_expected = 0;
    }
    const uint32_t revives[] = {202, 201}, potions[] = {104, 103, 102, 101};
    const uint32_t *items = hp.bits ? potions : revives;
    unsigned n = hp.bits ? 4 : 2;
    uint32_t item = 0;
    for (unsigned i = 0; i < n; i++) {
        uint32_t count = 0;
        if (!silph_game_read_item(game, (int32_t) items[i], &count) && count) {
            item = items[i];
            break;
        }
    }
    if (!item) {
        s->care_index++;
        __android_log_print(ANDROID_LOG_INFO, "SilphEnhancer",
                            "raid team care skipped; required medicine unavailable");
        return;
    }
    SilphManagedRef bag = {0}, promise = {0};
    r = silph_game_read_item_bag(game, &bag);
    if (!r)r = silph_rocket_medicine(read, permit, bag, id, item, &promise);
    void *object = NULL;
    if (!r)r = silph_managed_target(managed, promise, &object);
    if (!r && object && keep(s, object, &s->care_pending, false)) {
        s->care_target = id;
        s->care_hp = (int32_t) hp.bits;
        s->care_at = now;
        __android_log_print(ANDROID_LOG_INFO, "SilphEnhancer",
                            "raid team medicine requested item=%u", item);
    }
    else {
        s->care_index++;
        s->care_expected = 0;
    }
    if (promise.serial)silph_managed_release(managed, promise);
    if (bag.serial)silph_managed_release(managed, bag);
}
