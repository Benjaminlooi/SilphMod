static void raid_clear(Enhancer *s) {
#ifdef SILPH_PILOT_AUTORAID
    if(s->config.flags&SILPH_ENH_AUTORAID){drop(s,&s->route.boss_proto);s->route.boss_moves_ready=0;s->route.boss_moves_next=0;}
#endif
    if (s->care_count)s->care_ready = 1;
    s->raid_terminal_at = 0;
    s->score_ready = 0;
    s->raid_charge_turn = s->raid_dodge_for = s->raid_dodge_seen_turn = -1;
    s->raid_charge_move = 0;
    s->raid_event_types = 0;
    s->raid_clock_at = 0;
    s->raid_clock_turn = s->raid_last_dodge_turn = -1;
    s->raid_primary_events = s->raid_clock_debug = 0;
    if (s->raid_bound)
        __android_log_print(ANDROID_LOG_INFO, "SilphEnhancer",
                            "raid released status=%d actions=%llu dodges=%llu accepted=%llu rejected=%llu",
                            s->raid_status, (unsigned long long) s->raid_actions,
                            (unsigned long long) s->raid_dodges,
                            (unsigned long long) s->raid_accepted,
                            (unsigned long long) s->raid_rejected);
    drop(s, &s->raid);
    drop(s, &s->raid_pending);
    drop(s, &s->raid_move_table);
    drop(s, &s->raid_move_iterator);
    drop(s, &s->raid_actor);
    drop(s, &s->raid_local);
    s->raid_dodge_serial = -1;
    s->raid_active = 0;
    s->raid_updates = 0;
    s->raid_event_debug = 0;
    memset(s->raid_boss_moves, 0, sizeof(s->raid_boss_moves));
    s->raid_update_serial = s->raid_update_turn = -1;
    s->raid_moves_ready = 0;
    s->raid_bound = 0;
    s->raid_turn = -1;
    s->raid_action_until = 0;
    s->raid_serial = -1;
    s->dodge_start = s->dodge_end = 0;
    s->raid_next_at = 0;
    s->raid_manual_at = 0;
    s->raid_move_count = 0;
}

static void raid_enter(void *o, void *api, void *data, const void *m) {
    Enhancer *s = owned();
    ENH_SCOPE(s);
    raid_enter_original(o, api, data, m);
    if (allowed(s) && (s->metrics.available & SILPH_ENH_RAID) && exact(s, o, s->raid_class)) {
        raid_clear(s);
        bool retained = keep(s, o, &s->raid, true);
        drop(s, &s->manual_raid_gui);
        drop(s, &s->manual_raid_root);
        s->manual_raid_gym[0] = 0;
        s->manual_raid_next = 0;
        s->manual_raid_until = clock_ns() + UINT64_C(900000000000);
        __android_log_print(ANDROID_LOG_INFO, "SilphEnhancer",
                            "manual raid entered; team preserved retained=%u", retained);
    }
}

static void raid_exit(void *o, const void *m) {
    Enhancer *s = owned();
    ENH_SCOPE(s);
    void *active = NULL;
    if (s && target(s, s->raid, &active) && active == o)raid_clear(s);
    raid_exit_original(o, m);
}

static void *raid_submit(void *o, void *event, bool prediction, const void *m) {
    Enhancer *s = owned();
    ENH_SCOPE(s);
    if (allowed(s) && s->raid.serial && !s->raid_own) {
        void *raid = NULL, *director = NULL;
        if (target(s, s->raid, &raid)) {
            s->api.get(raid, s->raid_director, &director);
            int32_t kind = 0;
            if (director == o && read_i32(s, event, "Type", &kind, true) &&
                (kind == 7 || kind == 8 || kind == 9))
                s->raid_manual_at = clock_ns() + UINT64_C(1200000000);
        }
    }
    return raid_submit_original(o, event, prediction, m);
}

static void *find_nested(Enhancer *s, void *k, const char *name) {
    void *it = NULL;
    for (unsigned i = 0; k && i < 128; i++) {
        void *v = s->api.nested(k, &it);
        if (!v)return NULL;
        const char *n = s->api.class_name(v);
        if (n && !strcmp(n, name))return v;
    }
    return NULL;
}

static void *typed_dictionary(Enhancer *s, void *master, const char *name) {
    void *container = NULL;
    const void *f = master ? s->api.field(s->base.object_get_class(master), "ctym") : NULL;
    if (!f)return NULL;
    s->api.get(master, f, &container);
    if (!container)return NULL;
    void *k = klass(s, "holo-protos.dll", "Holoholo.Rpc", name), *key = k ? s->api.type_object(
            s->base.class_get_type(k)) : NULL;
    if (!key)return NULL;
    const char *arg[] = {"System.Type"};
    const void *get = method(s, s->base.object_get_class(container), "get_Item", NULL, 1, arg);
    void *args[] = {key}, *dictionary = NULL;
    return invoke(s, get, container, args, &dictionary) ? dictionary : NULL;
}

static void move_cache(Enhancer *s, void *master) {
    if (s->raid_moves_ready)return;
    void *iterator = NULL;
    if (!target(s, s->raid_move_iterator, &iterator)) {
        void *dictionary = typed_dictionary(s, master, "MoveSettingsProto"), *values = NULL;
        if (!dictionary || !read_ref(s, dictionary, "get_Values", &values) ||
            !read_ref(s, values, "GetEnumerator", &iterator) ||
            !keep(s, iterator, &s->raid_move_iterator, false))
            return;
    }
    void *k = s->base.object_get_class(iterator);
    const void *next = method(s, k, "MoveNext", "System.Boolean", 0, NULL), *get = method(s, k,
                                                                                          "get_Current",
                                                                                          NULL, 0,
                                                                                          NULL);
    if (!next || !get) {
        drop(s, &s->raid_move_iterator);
        return;
    }
    uint64_t began = clock_ns();
    for (unsigned i = 0; i < 24 && clock_ns() - began < UINT64_C(600000); i++) {
        bool more = false;
        if (!scalar(s, next, iterator, &more, 1))break;
        if (!more) {
            s->raid_moves_ready = 1;
            drop(s, &s->raid_move_iterator);
            __android_log_print(ANDROID_LOG_INFO, "SilphEnhancer",
                                "raid move timings ready count=%u", s->raid_move_count);
            break;
        }
        void *value = NULL;
        int32_t id = 0, start = 0, end = 0, duration = 0, delta = 0;
        if (invoke(s, get, iterator, NULL, &value) && value &&
            read_i32(s, value, "UniqueId", &id, true) &&
            read_i32(s, value, "DamageWindowStartMs", &start, false) &&
            read_i32(s, value, "DamageWindowEndMs", &end, false) && id > 0 && start >= 0 &&
            end >= start && end <= 30000 && s->raid_move_count < 1024) {
            read_i32(s, value, "DurationMs", &duration, false);
            read_i32(s, value, "EnergyDelta", &delta, false);
            float power = 0;
            const void *p = field(s, s->base.object_get_class(value), "Power", "System.Single");
            if (p)s->api.get(value, p, &power);
            s->raid_moves[s->raid_move_count++] = (EnhMove) {id, start, end, duration, delta,
                                                             power};
        }
    }
}

static bool move_window(Enhancer *s, void *master, int32_t id, int32_t *start, int32_t *end) {
    for (unsigned i = 0; i < s->raid_move_count; i++)
        if (s->raid_moves[i].id == id) {
            *start = s->raid_moves[i].start;
            *end = s->raid_moves[i].end;
            return *end > 0;
        }
    void *dictionary = NULL;
    if (!target(s, s->raid_move_table, &dictionary)) {
        dictionary = typed_dictionary(s, master, "MoveSettingsProto");
        if (!dictionary || !keep(s, dictionary, &s->raid_move_table, false))return false;
    }
    void *key = s->api.value_box(klass(s, "holo-protos.dll", "Holoholo.Rpc", "HoloPokemonMove"),
                                 &id);
    if (!key)return false;
    const char *arg[] = {"System.Object"};
    void *args[] = {key}, *settings = NULL;
    if (!invoke(s, method(s, s->base.object_get_class(dictionary), "get_Item", NULL, 1, arg),
                dictionary, args, &settings) || !settings)
        return false;
    if (!read_i32(s, settings, "DamageWindowStartMs", start, false) ||
        !read_i32(s, settings, "DamageWindowEndMs", end, false) || *start < 0 || *end < *start ||
        *end > 30000)
        return false;
    if (s->raid_move_count < 1024)
        s->raid_moves[s->raid_move_count++] = (EnhMove) {id, *start, *end, 0, 0, 0};
    return true;
}

static void
raid_events(Enhancer *s, void *director, void *response, const SilphSchedulePermit *permit) {
    if (!s || !s->raid.serial ||
        (permit ? !silph_schedule_permitted(permit, true) : !allowed(s)))
        return;
    void *raid = NULL, *bound = NULL, *master = NULL, *boss = NULL, *state = NULL, *events = NULL;
    if (!target(s, s->raid, &raid) || !target(s, s->raid_actor, &boss))return;
    s->api.get(raid, s->raid_director, &bound);
    s->api.get(raid, s->raid_master, &master);
    if (bound != director ||
        !object_field(s, response, "BattleState", "Holoholo.Rpc.BattleStateProto", &state))
        return;
    int64_t serial_key = 0, turn_key = 0;
    if (!read_i64(s, response, "Serial", &serial_key) ||
        !read_i64(s, response, "Turn", &turn_key) ||
        (serial_key == s->raid_update_serial && turn_key == s->raid_update_turn))
        return;
    s->raid_update_serial = serial_key;
    s->raid_update_turn = turn_key;
    int32_t ms = 0;
    if (!read_i32(s, state, "MsPerTurn", &ms, false) || ms < 10 || ms > 2000)return;
    const void *list = s->api.field(s->base.object_get_class(state), "Events");
    if (!list)return;
    s->api.get(state, list, &events);
    if (!events)return;
    int32_t count = 0;
    void *k = s->base.object_get_class(events);
    if (!scalar(s, method(s, k, "get_Count", "System.Int32", 0, NULL), events, &count, 4) ||
        count < 0 || count > 256)
        return;
    const char *index_type[] = {"System.Int32"};
    const void *get = method(s, k, "get_Item", NULL, 1, index_type);
    s->raid_updates++;
    if (s->raid_updates <= 2)
        __android_log_print(ANDROID_LOG_INFO, "SilphEnhancer",
                            "raid update observed events=%d turn=%lld", count,
                            (long long) turn_key);
    for (int32_t i = 0; i < count; i++) {
        void *args[] = {&i}, *event = NULL, *actor = NULL, *attack = NULL;
        if (!invoke(s, get, events, args, &event) || !event ||
            !object_field(s, event, "ActorId", "System.String", &actor))
            continue;
        int32_t event_type = 0;
        if (!read_i32(s, event, "Type", &event_type, true))continue;
        if (event_type > 0 && event_type < 64 &&
            !(s->raid_event_types & (UINT64_C(1) << event_type))) {
            s->raid_event_types |= UINT64_C(1) << event_type;
            __android_log_print(ANDROID_LOG_INFO, "SilphEnhancer", "raid event type=%d",
                                event_type);
        }
        void *local = NULL;
        if (event_type == 21 && target(s, s->raid_local, &local)) {
            int size = s->base.string_length(local), serial = 0;
            if (size > 0 && size <= 256 && s->base.string_length(actor) == size &&
                !memcmp(s->api.chars(local), s->api.chars(actor), (size_t) size * 2) &&
                read_i32(s, event, "Serial", &serial, false) &&
                s->raid_dodge_seen_turn != turn_key) {
                s->raid_dodge_seen_turn = turn_key;
                s->raid_dodge_serial = serial;
                s->raid_dodge_success++;
                __android_log_print(ANDROID_LOG_INFO, "SilphEnhancer",
                                    "raid dodge confirmed count=%llu",
                                    (unsigned long long) s->raid_dodge_success);
            }
            continue;
        }
        int32_t slot = 0, move = 0, serial = 0;
        int64_t turn = 0;
        if (event_type != 4 || !read_ref(s, event, "get_Attack", &attack) ||
            !read_i32(s, attack, "AttackType", &slot, true) ||
            !read_i32(s, attack, "Move", &move, true) ||
            !read_i32(s, event, "Serial", &serial, false) || !read_i64(s, event, "Turn", &turn))
            continue;
        int n = s->base.string_length(boss);
        bool enemy = n > 0 && n <= 256 && s->base.string_length(actor) == n &&
                     !memcmp(s->api.chars(boss), s->api.chars(actor), (size_t) n * 2);
        if (!enemy) {
            void *current_state = NULL, *all = NULL, *actor_state = NULL;
            int32_t ally = 0, team = 0;
            if (read_ref(s, director, "get_CurrentState", &current_state) &&
                read_i32(s, current_state, "AllyTeam", &ally, false)) {
                const void *f = s->api.field(s->base.object_get_class(current_state), "AllActors");
                if (f)s->api.get(current_state, f, &all);
                if (all) {
                    const char *key_type[] = {"System.String"};
                    void *key[] = {actor};
                    if (invoke(s, method(s, s->base.object_get_class(all), "get_Item", NULL, 1,
                                         key_type), all, key, &actor_state) && actor_state &&
                        scalar(s, method(s, s->base.object_get_class(actor_state), "get_Team",
                                         "System.Int32", 0, NULL), actor_state, &team, 4)) {
                        void *actor_proto = NULL;
                        int32_t role = 0;
                        if (read_ref(s, actor_state, "get_ActorProto", &actor_proto) &&
                            read_i32(s, actor_proto, "Type", &role, true))
                            enemy = team != ally && (role == 4 || role == 5);
                    }
                }
            }
        }
        if (s->raid_event_debug < 12) {
            s->raid_event_debug++;
            __android_log_print(ANDROID_LOG_INFO, "SilphEnhancer",
                                "raid attack observed slot=%d move=%d enemy=%u", slot, move, enemy);
        }
        if (!enemy)continue;
        if (turn <= 0)turn = turn_key;
        if (turn <= 0)continue;
        if (slot == 0 && move)
            for (unsigned j = 0; j < s->raid_move_count; j++)
                if (s->raid_moves[j].id == move && s->raid_moves[j].delta < 0) {
                    slot = 2;
                    break;
                }
        if (slot != 2 && slot != 3 && slot != 8)continue;
        if (!move && slot <= 3)move = s->raid_boss_moves[slot];
        int32_t start = 0, end = 0;
        if (!move_window(s, master, move, &start, &end)) {
            __android_log_print(ANDROID_LOG_INFO, "SilphEnhancer",
                                "raid charged timing unavailable slot=%d move=%d start=%d end=%d",
                                slot, move, start, end);
            continue;
        }
        else {
            if (s->raid_charge_turn == turn && s->raid_charge_move == move)continue;
            s->raid_primary_events = 1;
            s->raid_charge_turn = turn;
            s->raid_charge_move = move;
            s->raid_serial = serial;
            s->dodge_start = turn + start / ms;
            s->dodge_end = turn + (end + ms - 1) / ms;
            __android_log_print(ANDROID_LOG_INFO, "SilphEnhancer",
                                "raid charged event move=%d start=%lld end=%lld", move,
                                (long long) s->dodge_start, (long long) s->dodge_end);
        }
    }
}

static void raid_update0(void *o, void *p, const void *m) {
    Enhancer *s = owned();
    ENH_SCOPE(s);
    raid_update_original[0](o, p, m);
    raid_events(s, o, p, NULL);
}

static void raid_update1(void *o, void *p, const void *m) {
    Enhancer *s = owned();
    ENH_SCOPE(s);
    raid_update_original[1](o, p, m);
    raid_events(s, o, p, NULL);
}

static void raid_update2(void *o, void *p, const void *m) {
    Enhancer *s = owned();
    ENH_SCOPE(s);
    raid_update_original[2](o, p, m);
    raid_events(s, o, p, NULL);
}

static void raid_update3(void *o, void *p, const void *m) {
    Enhancer *s = owned();
    ENH_SCOPE(s);
    raid_update_original[3](o, p, m);
    raid_events(s, o, p, NULL);
}

static void raid_new_turn(void *o, int64_t turn, const void *m) {
    Enhancer *s = owned();
    ENH_SCOPE(s);
    raid_turn_original(o, turn, m);
    if (!s || !s->raid.serial || turn < 0)return;
    void *raid = NULL, *director = NULL;
    if (!target(s, s->raid, &raid))return;
    s->api.get(raid, s->raid_director, &director);
    if (director == o) {
        s->raid_clock_turn = turn;
        s->raid_clock_at = clock_ns();
    }
}

static bool set_enum(Enhancer *s, void *o, const char *name, int32_t value) {
    const void *f = o ? enum_field(s, s->base.object_get_class(o), name) : NULL;
    if (!f)return false;
    s->api.set(o, f, &value);
    return true;
}

static bool
raid_send(Enhancer *s, const SilphSchedulePermit *permit, void *director, void *boss_actor,
          int32_t slot, void *move, int64_t turn, int32_t duration, uint64_t now) {
    void *event = new_value(s, s->event_class), *payload = new_value(s, slot == 5 ? s->dodge_class
                                                                                  : s->attack_class);
    if (!event || !payload || !set_enum(s, event, "Type", slot == 5 ? 5 : 4))return false;
    const void *setter = NULL;
    if (slot == 5) {
        if (!set_enum(s, payload, "Direction", 2))return false;
        setter = s->api.method(s->event_class, "set_Dodge", 1);
    }
    else {
        int32_t move_id = 0, move_type = 0;
        if (!read_i32(s, move, "Move", &move_id, true) ||
            !read_i32(s, move, "Type", &move_type, true) ||
            !set_enum(s, payload, "AttackType", slot) || !set_enum(s, payload, "Move", move_id) ||
            !set_enum(s, payload, "Type", move_type))
            return false;
        void *id = NULL;
        if (!read_ref(s, boss_actor, "get_ActorId", &id))return false;
        const void *target_field = field(s, s->attack_class, "TargetId", "System.String");
        if (!target_field)return false;
        s->api.set_object(payload, target_field, id);
        setter = s->api.method(s->event_class, "set_Attack", 1);
    }
    if (!setter || s->base.method_get_param_count(setter) != 1 ||
        s->api.class_from_type(s->api.parameter(setter, 0)) !=
        (slot == 5 ? s->dodge_class : s->attack_class) ||
        !type(s, s->base.method_get_return_type(setter), "System.Void"))
        return false;
    void *a[] = {payload};
    if (!invoke(s, setter, event, a, NULL))return false;
    bool prediction = slot == 5;
    void *args[] = {event, &prediction}, *promise = NULL;
    if (!silph_schedule_permitted(permit, true))return false;
    s->raid_own = 1;
    bool ok = invoke(s, s->submit_action, director, args, &promise);
    s->raid_own = 0;
    if (!ok || !promise || !keep(s, promise, &s->raid_pending, false))return false;
    s->raid_pending_at = now;
    s->raid_pending_type = (unsigned) slot;
    s->raid_pending_charge_turn = s->raid_charge_turn;
    s->raid_actions++;
    s->raid_turn = turn;
    s->raid_action_until = turn + (duration > 0 ? duration : 1);
    if (slot == 5) {
        s->raid_dodges++;
        s->raid_last_dodge_turn = turn;
    }
    if (slot != 1 || s->raid_actions == 1)
        __android_log_print(ANDROID_LOG_INFO, "SilphEnhancer",
                            "raid action type=%d turn=%lld duration=%d", slot, (long long) turn,
                            duration);
    return true;
}

static void raid_consider_action(Enhancer *s, void *master, void *action, int64_t turn, int64_t ms,
                                 bool queued) {
    int32_t variety = 0, move = 0;
    int64_t at = 0;
    if (!read_i32(s, action, "AttackVariety", &variety, true) ||
        (variety != 2 && variety != 3 && variety != 8) ||
        !read_i32(s, action, "AttackMove", &move, true) || !read_i64(s, action, "Turn", &at) ||
        at <= 0 || (s->raid_charge_turn == at && s->raid_charge_move == move))
        return;
    int32_t first = 0, last = 0;
    if (!move_window(s, master, move, &first, &last))return;
    int64_t start = at + first / ms, end = at + (last + ms - 1) / ms;
    if (end < turn || start > turn + 60 ||
        (s->dodge_end >= turn && s->raid_dodge_for != s->raid_charge_turn &&
         start > s->dodge_start))
        return;
    s->raid_charge_turn = at;
    s->raid_charge_move = move;
    s->dodge_start = start;
    s->dodge_end = end;
    __android_log_print(ANDROID_LOG_INFO, "SilphEnhancer",
                        "raid charged window move=%d current=%lld start=%lld end=%lld queued=%u",
                        move, (long long) turn, (long long) start, (long long) end, queued);
}

#ifdef SILPH_PILOT_AUTORAID
#include "autoraid-moves.inc"
#endif

static void raid_tick(Enhancer *s, const SilphSchedulePermit *permit, uint64_t now) {
    if (!s->raid.serial || !(s->metrics.available & SILPH_ENH_RAID) ||
        !silph_schedule_permitted(permit, true))
        return;
    void *raid = NULL, *director = NULL, *manager = NULL, *master = NULL, *state = NULL, *sync = NULL, *player = NULL, *boss = NULL, *player_state = NULL, *boss_state = NULL, *actor = NULL;
    if (!target(s, s->raid, &raid)) {
        __android_log_print(ANDROID_LOG_INFO, "SilphEnhancer", "raid state reference expired");
        raid_clear(s);
        return;
    }
    s->api.get(raid, s->raid_director, &director);
    s->api.get(raid, s->raid_manager, &manager);
    s->api.get(raid, s->raid_master, &master);
    if (!exact(s, director, s->director_class) || !exact(s, manager, s->raid_manager_class) ||
        !read_ref(s, director, "get_CurrentState", &state)) {
        if (s->raid_gate != 1) {
            s->raid_gate = 1;
            __android_log_print(ANDROID_LOG_INFO, "SilphEnhancer",
                                "raid binding waiting director=%s manager=%s state=%u",
                                director ? s->api.class_name(s->base.object_get_class(director))
                                         : "null",
                                manager ? s->api.class_name(s->base.object_get_class(manager))
                                        : "null", state != NULL);
        }
        return;
    }
    move_cache(s, master);
    int32_t status = 0;
    if (!read_i32(s, state, "BattleStatus", &status, true)) {
        if (s->raid_gate != 3) {
            s->raid_gate = 3;
            const void *f = s->api.field(s->base.object_get_class(state), "BattleStatus");
            const char *name = f ? s->api.type_name(s->api.field_type(f)) : NULL;
            __android_log_print(ANDROID_LOG_INFO, "SilphEnhancer",
                                "raid status contract state=%s field=%s",
                                s->api.class_name(s->base.object_get_class(state)),
                                name ? name : "missing");
            if (name)s->api.free((void *) name);
        }
        return;
    }
    if (status != s->raid_status) {
        s->raid_status = status;
        __android_log_print(ANDROID_LOG_INFO, "SilphEnhancer",
                            "raid state=%d actions=%llu dodges=%llu", status,
                            (unsigned long long) s->raid_actions,
                            (unsigned long long) s->raid_dodges);
    }
    if (status != 1) {
        drop(s, &s->raid_pending);
        if (status > 1 && s->care_count)s->care_ready = 1;
        return;
    }
    bool started = false, blocked = true;
    if (!scalar(s, method(s, s->director_class, "get_HasBattleStarted", "System.Boolean", 0, NULL),
                director, &started, 1) || !started)
        return;
    if (!read_ref(s, state, "get_CurrentTurnSyncInfo", &sync))return;
    bool valid = false;
    int64_t turn = 0, ms = 0;
    if (!scalar(s,
                method(s, s->base.object_get_class(sync), "get_SyncedTurnValid", "System.Boolean",
                       0, NULL), sync, &valid, 1) || !valid || !scalar(s, method(s,
                                                                                 s->base.object_get_class(
                                                                                         sync),
                                                                                 "get_SyncedTurn",
                                                                                 "System.Int64", 0,
                                                                                 NULL), sync, &turn,
                                                                       8) || turn < 0 ||
        !read_i64(s, sync, "MsPerTurn", &ms) || ms < 10 || ms > 2000)
        return;
    if (s->raid_clock_at && now >= s->raid_clock_at &&
        now - s->raid_clock_at <= UINT64_C(1500000000) && s->raid_clock_turn >= turn &&
        s->raid_clock_turn - turn <= 6) {
        if (s->raid_clock_debug < 2) {
            s->raid_clock_debug++;
            __android_log_print(ANDROID_LOG_INFO, "SilphEnhancer",
                                "raid turn clocks synced=%lld current=%lld", (long long) turn,
                                (long long) s->raid_clock_turn);
        }
        turn = s->raid_clock_turn;
    }
    if (!read_ref(s, manager, "get_PlayerBattlePokemon", &player) ||
        !read_ref(s, manager, "get_RaidBossPokemon", &boss) ||
        !read_ref(s, player, "get_BattlePokemonState", &player_state) ||
        !read_ref(s, boss, "get_BattlePokemonState", &boss_state) ||
        !object_field(s, boss_state, "Actor", "Niantic.Holoholo.VNext.BattleActorState", &actor))
        return;
    void *boss_proto = NULL, *boss_moves = NULL;
    if (read_ref(s, boss_state, "get_Pokemon", &boss_proto)) {
        const void *f = s->api.field(s->base.object_get_class(boss_proto), "Moves");
        if (f)s->api.get(boss_proto, f, &boss_moves);
        bool refresh = true;
#ifdef SILPH_PILOT_AUTORAID
                                                                                                                                if(s->config.flags&SILPH_ENH_AUTORAID){void *cached=NULL;bool same=target(s,s->route.boss_proto,&cached)&&cached==boss_proto;
            refresh=!same||(!s->route.boss_moves_ready&&now>=s->route.boss_moves_next);
            if(!same){memset(s->raid_boss_moves,0,sizeof(s->raid_boss_moves));keep(s,boss_proto,&s->route.boss_proto,true);s->route.boss_moves_ready=0;}
        }
#endif
        if (refresh && boss_moves) {
            const char *key_type[] = {"System.Int32"};
            const void *get = method(s, s->base.object_get_class(boss_moves), "get_Item", NULL, 1,
                                     key_type);
            for (int32_t slot = 1; slot <= 3; slot++) {
                void *key[] = {&slot}, *move = NULL;
                int32_t id = 0;
                if (invoke(s, get, boss_moves, key, &move) && move &&
                    read_i32(s, move, "Move", &id, true) && id)
                    s->raid_boss_moves[slot] = id;
            }
        }
#ifdef SILPH_PILOT_AUTORAID
        if((s->config.flags&SILPH_ENH_AUTORAID)&&refresh){s->route.boss_moves_ready=s->raid_boss_moves[1]&&s->raid_boss_moves[2];s->route.boss_moves_next=now+UINT64_C(1000000000);}
#endif
    }
    void *boss_id = NULL, *previous = NULL;
    if (read_ref(s, actor, "get_ActorId", &boss_id) &&
        (!target(s, s->raid_actor, &previous) || previous != boss_id))
        keep(s, boss_id, &s->raid_actor, false);
    void *local_actor = NULL, *local_id = NULL;
    if (object_field(s, player_state, "Actor", "Niantic.Holoholo.VNext.BattleActorState",
                     &local_actor) && read_ref(s, local_actor, "get_ActorId", &local_id) &&
        (!target(s, s->raid_local, &previous) || previous != local_id))
        keep(s, local_id, &s->raid_local, false);
    void *update = NULL;
    if (object_field(s, director, "equz", "Holoholo.Rpc.BattleStateOutProto", &update))
        raid_events(s, director, update, permit);
    if (!s->raid_bound) {
        s->raid_bound = 1;
        void *local = NULL;
        if (read_ref(s, state, "get_LocalActor", &local))care_track(s, local);
        __android_log_print(ANDROID_LOG_INFO, "SilphEnhancer", "normal raid bound msPerTurn=%lld",
                            (long long) ms);
    }
    void *action = NULL;
    if (!s->raid_primary_events &&
        object_field(s, boss_state, "CurrentAction", "Niantic.Holoholo.VNext.BattleTurnAction",
                     &action))
        raid_consider_action(s, master, action, turn, ms, false);
    if (!s->raid_primary_events &&
        (s->dodge_end < turn || s->raid_dodge_for == s->raid_charge_turn)) {
        const char *names[] = {"QueuedActions", "ActionsVisuallyPending"};
        for (unsigned j = 0; j < 2; j++) {
            void *list = NULL;
            const void *f = field(s, s->base.object_get_class(boss_state), names[j],
                                  "System.Collections.Generic.List<Niantic.Holoholo.VNext.BattleTurnAction>");
            if (!f)continue;
            s->api.get(boss_state, f, &list);
            if (!list)continue;
            int32_t count = 0;
            void *k = s->base.object_get_class(list);
            if (!scalar(s, method(s, k, "get_Count", "System.Int32", 0, NULL), list, &count, 4) ||
                count < 0 || count > 64)
                continue;
            const char *index[] = {"System.Int32"};
            const void *get = method(s, k, "get_Item", NULL, 1, index);
            for (int32_t i = 0; i < count && i < 8; i++) {
                void *args[] = {&i}, *queued = NULL;
                if (invoke(s, get, list, args, &queued) && queued)
                    raid_consider_action(s, master, queued, turn, ms, true);
            }
        }
    }
    void *current_pokemon = NULL;
    if (read_ref(s, player_state, "get_Pokemon", &current_pokemon)) {
        const void *id_field = field(s, s->base.object_get_class(current_pokemon), "Id",
                                     "System.UInt64");
        uint64_t id = 0;
        if (id_field)s->api.get(current_pokemon, id_field, &id);
        if (id && id != s->raid_active) {
            s->raid_active = id;
            void *local = NULL;
            if (read_ref(s, state, "get_LocalActor", &local))care_track(s, local);
            drop(s, &s->raid_pending);
            s->raid_action_until = turn;
            s->raid_turn = turn - 1;
            s->raid_next_at = 0;
            __android_log_print(ANDROID_LOG_INFO, "SilphEnhancer",
                                "raid active team member updated");
        }
    }
    bool dodge_due = s->raid_dodge_for != s->raid_charge_turn && s->dodge_end > 0 &&
                     turn >= s->dodge_start && turn <= s->dodge_end;
    if (dodge_due && s->raid_pending.serial) {
        void *promise = NULL;
        bool accepted = false;
        if (s->raid_pending_type == 5 && target(s, s->raid_pending, &promise)) {
            const void *done = field(s, s->base.object_get_class(promise), "completeCalled",
                                     "System.Boolean"), *error = field(s, s->base.object_get_class(
                    promise), "errorCalled", "System.Boolean"), *value = enum_field(s,
                                                                                    s->base.object_get_class(
                                                                                            promise),
                                                                                    "completedValue");
            bool complete = false, failed = false;
            int32_t result = 0;
            if (done && error && value) {
                s->api.get(promise, done, &complete);
                s->api.get(promise, error, &failed);
                if (complete && !failed)s->api.get(promise, value, &result);
                accepted = complete && !failed && (result == 1 || result == 6);
            }
        }
        if (accepted) {
            s->raid_dodge_for = s->raid_pending_charge_turn;
            dodge_due = s->raid_dodge_for != s->raid_charge_turn;
        }
        if (dodge_due && s->raid_last_dodge_turn != turn) {
            drop(s, &s->raid_pending);
            s->raid_action_until = turn;
            s->raid_next_at = 0;
        }
    }
    if (dodge_due && s->raid_last_dodge_turn != turn && now >= s->raid_manual_at) {
        int32_t hp = 0;
        if (read_i32(s, player_state, "CurrentHealth", &hp, false) && hp > 0) {
            raid_send(s, permit, director, actor, 5, NULL, turn, 1, now);
            return;
        }
    }
    if (dodge_due)return;
    if (s->raid_pending.serial) {
        void *promise = NULL;
        if (target(s, s->raid_pending, &promise)) {
            const void *complete = field(s, s->base.object_get_class(promise), "completeCalled",
                                         "System.Boolean"), *error = field(s,
                                                                           s->base.object_get_class(
                                                                                   promise),
                                                                           "errorCalled",
                                                                           "System.Boolean");
            bool done = false, failed = false;
            if (complete && error) {
                s->api.get(promise, complete, &done);
                s->api.get(promise, error, &failed);
                if (!done && !failed && now - s->raid_pending_at < UINT64_C(10000000000) &&
                    turn < s->raid_action_until)
                    return;
                int32_t result = 0;
                const void *f = enum_field(s, s->base.object_get_class(promise), "completedValue");
                if (done && !failed && f)s->api.get(promise, f, &result);
                if (result == 1) {
                    s->raid_accepted++;
                    if (s->raid_pending_type == 5)s->raid_dodge_for = s->raid_pending_charge_turn;
                }
                else {
                    s->raid_rejected++;
                    if (s->raid_rejected <= 3)
                        __android_log_print(ANDROID_LOG_INFO, "SilphEnhancer",
                                            "raid action response type=%u result=%d",
                                            s->raid_pending_type, result);
                    s->raid_next_at = now + UINT64_C(300000000);
                    s->raid_action_until = turn;
                }
            }
        }
        drop(s, &s->raid_pending);
    }
    if (now < s->raid_manual_at || now < s->raid_next_at || turn <= s->raid_turn ||
        turn < s->raid_action_until)
        return;
    if (!scalar(s, method(s, s->director_class, "get_InputBlocked", "System.Boolean", 0, NULL),
                director, &blocked, 1))
        return;
    int32_t hp = 0, energy = 0;
    if (!read_i32(s, player_state, "CurrentHealth", &hp, false) || hp <= 0 ||
        !read_i32(s, player_state, "CurrentEnergy", &energy, false))
        return;
    void *pokemon = NULL, *moves = NULL;
    if (!read_ref(s, player_state, "get_Pokemon", &pokemon))return;
    const void *moves_field = s->api.field(s->base.object_get_class(pokemon), "Moves");
    if (!moves_field)return;
    s->api.get(pokemon, moves_field, &moves);
    if (!moves)return;
    const char *arg[] = {"System.Int32"};
    const void *get = method(s, s->base.object_get_class(moves), "get_Item", NULL, 1, arg);
    void *selected = NULL;
    int32_t chosen = 0, duration = 0;
    for (int32_t slot = 1; slot <= 3; slot++) {
        void *in[] = {&slot}, *candidate = NULL;
        if (!invoke(s, get, moves, in, &candidate) || !candidate)continue;
        int32_t delta = 0, turns = 0, id = 0;
        if (!read_i32(s, candidate, "EnergyDelta", &delta, false) ||
            !read_i32(s, candidate, "DurationTurns", &turns, false) ||
            !read_i32(s, candidate, "Move", &id, true))
            continue;
        for (unsigned j = 0; (turns < 1 || delta == 0) && j < s->raid_move_count; j++)
            if (s->raid_moves[j].id == id) {
                if (turns < 1 && s->raid_moves[j].duration > 0)
                    turns = (s->raid_moves[j].duration + (int32_t) ms - 1) / (int32_t) ms;
                if (!delta)delta = s->raid_moves[j].delta;
                break;
            }
        if (turns < 1 || turns > 500)continue;
        if (slot == 1 || (delta < 0 && energy >= -delta)) {
            selected = candidate;
            chosen = slot;
            duration = turns;
            if (slot > 1)break;
        }
    }
#ifdef SILPH_PILOT_AUTORAID
    if(ar_choose_move(s,master,pokemon,boss_proto,moves,energy,turn,ms,&selected,&chosen,&duration)&&!selected)return;
#endif
    if (!selected) {
        if (s->raid_gate != 12) {
            s->raid_gate = 12;
            const void *candidate = s->api.method(s->base.object_get_class(moves), "get_Item", 1);
            const char *p = candidate ? s->api.type_name(s->api.parameter(candidate, 0)) : NULL;
            __android_log_print(ANDROID_LOG_INFO, "SilphEnhancer",
                                "raid moves waiting getter=%u key=%s energy=%d", get != NULL,
                                p ? p : "missing", energy);
            if (p)s->api.free((void *) p);
        }
        return;
    }
    if (s->raid_dodge_for != s->raid_charge_turn && s->dodge_start > turn &&
        turn + duration + 2 >= s->dodge_start)
        return;
    if (!raid_send(s, permit, director, actor, chosen, selected, turn, duration, now) &&
        s->raid_gate != 13) {
        s->raid_gate = 13;
        __android_log_print(ANDROID_LOG_INFO, "SilphEnhancer",
                            "raid action contract unavailable slot=%d", chosen);
    }
}

static void raid_open(Enhancer *s) {
    if (!(s->config.flags & SILPH_ENH_RAID))return;
    const char *space = "Niantic.Holoholo.VNext", *assembly = "holo-game-vnext-battle.dll";
    s->raid_class = klass(s, assembly, space, "RaidBattleVNextState");
    s->director_class = klass(s, assembly, space, "BattleDirector");
    s->raid_manager_class = klass(s, assembly, space, "RaidBattlePokemonManager");
    s->raid_director = field(s, s->raid_class, "eqqh", "Niantic.Holoholo.VNext.IBattleDirector");
    s->raid_manager = field(s, s->raid_class, "eqpl",
                            "Niantic.Holoholo.VNext.IRaidBattlePokemonManager");
    s->raid_master = field(s, s->raid_class, "eqpz", "IGameMasterData");
    s->event_class = klass(s, "holo-protos.dll", "Holoholo.Rpc", "BattleEventProto");
    void *types = find_nested(s, s->event_class, "Types");
    s->attack_class = find_nested(s, types, "Attack");
    s->dodge_class = find_nested(s, types, "Dodge");
    const char *enter_args[] = {"Niantic.Platform.GameStates.StateLogic.IStateApi",
                                "Niantic.Holoholo.Raid.RaidBattleLaunchData"}, *submit_args[] = {
            "Holoholo.Rpc.BattleEventProto", "System.Boolean"};
    const void *entry = method(s, s->raid_class, "EnterState", "System.Void", 2,
                               enter_args), *exit = method(s, s->raid_class, "ExitState",
                                                           "System.Void", 0, NULL);
    s->submit_action = method(s, s->director_class, "TrySubmitPlayerAction", NULL, 2, submit_args);
    if (!entry || !exit || !s->submit_action || !s->raid_director || !s->raid_manager ||
        !s->raid_master || !s->event_class || !s->attack_class || !s->dodge_class)
        return;
    if (hook(s, entry, (void *) raid_enter, (void **) &raid_enter_original) ||
        hook(s, exit, (void *) raid_exit, (void **) &raid_exit_original) ||
        hook(s, s->submit_action, (void *) raid_submit, (void **) &raid_submit_original))
        return;
    const char *update_args[] = {"Holoholo.Rpc.BattleStateOutProto"};
    const void *updates[4] = {0};
    void *it = NULL;
    unsigned count = 0;
    for (unsigned i = 0; i < 512; i++) {
        const void *m = s->base.class_get_methods(s->director_class, &it);
        if (!m)break;
        const char *name = s->base.method_get_name(m);
        if (name && method(s, s->director_class, name, "System.Void", 1, update_args) == m) {
            if (count == 4)return;
            updates[count++] = m;
        }
    }
    void *proxies[] = {(void *) raid_update0, (void *) raid_update1, (void *) raid_update2,
                       (void *) raid_update3};
    for (unsigned i = 0; i < count; i++)
        if (hook(s, updates[i], proxies[i], (void **) &raid_update_original[i]))return;
    const char *turn_args[] = {"System.Int64"};
    const void *turn = method(s, s->director_class, "crcj", "System.Void", 1, turn_args);
    if (turn && hook(s, turn, (void *) raid_new_turn, (void **) &raid_turn_original))return;
    if (count)s->metrics.available |= SILPH_ENH_RAID;
}
