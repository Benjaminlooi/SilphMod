#include "raid-performance-policy.h"

static void
ar_phase(Enhancer *s, unsigned phase, unsigned stage, unsigned reason, uint64_t deadline) {
    AutoRaid *a = &s->route;
    if (a->phase == phase && a->stage == stage && a->reason == reason)return;
    if (phase == 5 && (a->phase != 5 || a->stage != stage)) {
        uint32_t wait = silph_raid_stage_wait_ms(stage);
        if (wait)deadline = clock_ns() + (uint64_t) wait * 1000000;
    }
    if (phase == 7 && a->phase != 7) {
        SilphManagedMetrics refs = {0};
        if (!silph_managed_metrics(s->managed, &refs))
            __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                                "raid boundary request=%llu liveRefs=%u created=%llu released=%llu",
                                (unsigned long long) a->request, refs.live_refs,
                                (unsigned long long) refs.refs_created,
                                (unsigned long long) refs.refs_released);
    }
    a->phase = phase;
    a->stage = stage;
    a->reason = reason;
    a->deadline = deadline;
    a->dirty = 1;
    __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                        "phase=%u stage=%u reason=%u request=%llu gym=%s", phase, stage, reason,
                        (unsigned long long) a->request, a->site.gym);
}

static int ar_service(Enhancer *s, unsigned kind, SilphManagedRef *out) {
    AutoRaid *a = &s->route;
    SilphManagedRef ref = {0};
    void *object = NULL;
    int r = silph_game_read_raid_service(a->game, kind, &ref);
    if (!r)r = silph_managed_target(a->managed, ref, &object);
    if (!r && !keep(s, object, out, false))r = SILPH_MANAGED_CAPACITY;
    if (ref.serial) {
        int freed = silph_managed_release(a->managed, ref);
        if (!r)r = freed;
    }
    return r;
}

int silph_enhancer_route_bind(void *memory, void *read, void *managed, void *game, void *link,
                              const SilphScheduleIdentity *identity) {
    Enhancer *s = memory;
    if (!s || !identity)return 1;
    if (!(s->config.flags & SILPH_ENH_AUTORAID))return 0;
    AutoRaid *a = &s->route;
    if (a->link)return memcmp(&a->identity, identity, sizeof(*identity)) ? SILPH_MANAGED_STALE : 0;
    a->read = read;
    a->managed = managed;
    a->game = game;
    a->link = link;
    a->identity = *identity;
    a->request = 1;
    a->map_pokemon_class = klass(s, "holo-game.dll", "Niantic.Holoholo.Map", "RaidMapPokemon");
    if (!a->map_pokemon_class)return SILPH_MANAGED_CONTRACT;
    if (!s->ball_class)
        s->ball_class = klass(s, "holo-game.dll", "Niantic.Holoholo.Encounter", "Pokeball");
    if (!s->ball_type)
        s->ball_type = method(s, s->ball_class, "get_BallType", "Holoholo.Rpc.Item", 0, NULL);
    if (!s->interaction_ball)
        s->interaction_ball = method(s, s->interaction_class, "get_Pokeball",
                                     "Niantic.Holoholo.Encounter.IPokeball", 0, NULL);
    if (!s->ball_type || !s->interaction_ball)return SILPH_MANAGED_CONTRACT;
    const char *capture_types[] = {"Niantic.Holoholo.Encounter.PokeballThrow",
                                   "Holoholo.Rpc.ARPlusEncounterValuesProto"};
    a->capture = method(s, a->map_pokemon_class, "TryCapture", NULL, 2, capture_types);
    if (!a->capture)return SILPH_MANAGED_CONTRACT;
    SilphManagedRef map = {0}, rpc = {0};
    void *object = NULL;
    int r = silph_game_read_raid_service(game, 4, &map);
    if (!r)r = silph_managed_target(managed, map, &object);
    a->reader = calloc(1, silph_raid_read_size());
    a->pass = calloc(1, silph_raid_pass_size());
    if (!a->reader || !a->pass)r = SILPH_MANAGED_CAPACITY;
    if (!r)r = silph_raid_read_open(a->reader, silph_raid_read_size(), read, managed, object);
    if (!r)r = silph_game_read_raid_service(game, 0, &rpc);
    if (!r)r = silph_raid_pass_open(a->pass, silph_raid_pass_size(), read, managed, rpc, identity);
    if (map.serial)silph_managed_release(managed, map);
    if (rpc.serial)silph_managed_release(managed, rpc);
    if (!r)r = ar_service(s, 1, &a->explore);
    if (!r)r = ar_service(s, 2, &a->root);
    if (!r)r = ar_service(s, 3, &a->gui);
    if (!r) {
        SilphManagedRef container = {0};
        void *di = NULL, *factory = NULL, *kind = find_nested(s, a->map_pokemon_class, "Factory");
        r = ar_service(s, 5, &container);
        void *type_object = kind ? s->api.type_object(s->base.class_get_type(kind)) : NULL;
        const char *types[] = {"System.Type"};
        void *args[] = {type_object};
        if (!r && (!type_object || !target(s, container, &di) ||
                   !invoke(s, method(s, s->base.object_get_class(di), "TryResolve", NULL, 1, types),
                           di, args, &factory) || !factory ||
                   !keep(s, factory, &a->factory, false)))
            r = SILPH_MANAGED_CONTRACT;
        const char *reward_types[] = {"Holoholo.Rpc.RaidEncounterProto"};
        if (!r) {
            a->create_model = method(s, s->base.object_get_class(factory), "Create",
                                     "Niantic.Holoholo.Map.RaidMapPokemon", 1, reward_types);
            if (!a->create_model)r = SILPH_MANAGED_CONTRACT;
        }
        drop(s, &container);
        if (r)
            __android_log_print(ANDROID_LOG_ERROR, "SilphRaid",
                                "capture model factory unavailable error=%d", r);
    }
    if (!r) {
        silph_game_read_position(game, &a->latitude, &a->longitude);
        ar_phase(s, 1, 0, 0, 0);
    }
    return r;
}

int silph_enhancer_route_publish(void *memory) {
    Enhancer *s = memory;
    if (!s || !(s->config.flags & SILPH_ENH_AUTORAID))return 0;
    AutoRaid *a = &s->route;
    if (!a->dirty || !a->link)return 0;
    SilphKecleonEvent event = {.identity=a->identity, .sequence=a->event_sequence +
                                                                1, .request=a->request, .phase=a->phase, .move=
    a->phase == 6 ? 1 : 0, .reason=a->reason, .verified=a->won && (a->phase == 4 || a->phase == 5 ||
                                                                   a->phase == 7), .latitude=
    a->phase == 4 ? a->capture_latitude : a->latitude, .longitude=a->phase == 4
                                                                  ? a->capture_longitude
                                                                  : a->longitude};
    if (a->phase != 1 && a->phase != 8) {
        memcpy(event.source, a->source, sizeof(event.source));
        memcpy(event.fort, a->site.gym, sizeof(event.fort));
        if (a->site.seed)
            snprintf(event.incident, sizeof(event.incident), "%llu",
                     (unsigned long long) a->site.seed);
    }
    if (a->phase == 1 && a->survey_row) {
        SilphRaidSite *v = &a->survey_site;
        event.move = 2;
        event.verified = 1;
        event.latitude = v->latitude;
        event.longitude = v->longitude;
        snprintf(event.source, sizeof(event.source), "%u:%u:%u:%u:%llu:%u", v->species, v->level,
                 v->form, v->costume, (unsigned long long) v->end_ms,
                 v->evolution_known ? v->evolution : 0);
        memcpy(event.fort, v->gym, sizeof(event.fort));
        snprintf(event.incident, sizeof(event.incident), "%llu", (unsigned long long) v->seed);
    } else if (a->phase == 1 && a->survey_done) {
        event.move = 1;
        event.reason = a->survey_error ? 3 : 0;
    }
    int r = silph_session_link_event(a->link, &event);
    if (r == SILPH_SCHEDULE_BUSY)return 0;
    if (!r) {
        a->dirty = 0;
        a->event_sequence++;
    }
    return r;
}

static bool ar_void(Enhancer *s, void *object, const char *name) {
    return object && invoke(s, method(s, s->base.object_get_class(object), name, "System.Void", 0,
                                      NULL), object, NULL, NULL);
}

static void ar_exit(Enhancer *s) {
    void *root = NULL;
    if (!target(s, s->route.root, &root))return;
    const char *types[] = {"Holoholo.Rpc.RaidEncounterProto", "Holoholo.Rpc.GetRaidDetailsOutProto",
                           "Niantic.Holoholo.Raid.RaidResolveInformation"};
    void *args[] = {NULL, NULL, NULL};
    invoke(s, method(s, s->base.object_get_class(root), "ExitGym", "System.Void", 3, types), root,
           args, NULL);
}

static void ar_done(Enhancer *s, unsigned reason) {
    AutoRaid *a = &s->route;
    if (a->phase != 5) {
        ar_phase(s, 7, 0, reason, 0);
        return;
    }
    if (a->stage == 8)return;
    a->leave_reason = reason;
    a->restore_attempts = 0;
    a->restore_at = 0;
    ar_exit(s);
    ar_phase(s, 5, 8, reason, clock_ns() + UINT64_C(20000000000));
}

static bool ar_owns_gui(Enhancer *s) {
    void *root = NULL, *fort = NULL;
    char gym[161] = {0};
    return target(s, s->route.root, &root) && read_ref(s, root, "get_FortId", &fort) &&
           label_string(s, fort, gym, sizeof(gym)) && !strcmp(gym, s->route.site.gym);
}

static bool ar_map_restored(Enhancer *s) {
    void *root = NULL, *service = NULL, *gui = NULL;
    bool active = true;
    if (!s->map || !target(s, s->route.root, &root) || !scalar(s, method(s,
                                                                         s->base.object_get_class(
                                                                                 root),
                                                                         "get_IsActive",
                                                                         "System.Boolean", 0, NULL),
                                                               root, &active, 1) || active)
        return false;
    if (!target(s, s->route.gui, &service) || !read_ref(s, service, "TopGui", &gui))return false;
    const char *name = s->api.class_name(s->base.object_get_class(gui));
    return name && !strcmp(name, "MainMenuGuiController");
}

static void ar_vista(Enhancer *s, bool restore) {
    AutoRaid *a = &s->route;
    void *vista = NULL;
    if (!a->vista_checked && !restore) {
        a->vista_checked = 1;
        SilphManagedRef container = {0};
        void *di = NULL, *service = NULL;
        int r = ar_service(s, 5, &container);
        if (!r && !target(s, container, &di))r = SILPH_MANAGED_STALE;
        void *k = klass(s, "holo-game-interfaces.dll", "Niantic.Holoholo.Map",
                        "IEnvironmentDataService"), *kind = k ? s->api.type_object(
                s->base.class_get_type(k)) : NULL;
        const char *types[] = {"System.Type"};
        void *args[] = {kind};
        if (!r && (!kind ||
                   !invoke(s, method(s, s->base.object_get_class(di), "TryResolve", NULL, 1, types),
                           di, args, &service) || !service))
            r = SILPH_MANAGED_NOT_READY;
        if (!r && !invoke(s, unique_method(s, s->base.object_get_class(service),
                                           "Holoholo.Rpc.VistaGeneralSettingsProto", 0, NULL),
                          service, NULL, &vista))
            r = SILPH_MANAGED_CONTRACT;
        if (!r && vista && keep(s, vista, &a->vista, false)) {
            const char *names[] = {"IsVistaMapEnabled", "IsVistaBattleEnabled",
                                   "IsVistaEncountersEnabled"};
            for (unsigned i = 0; i < 3; i++) {
                a->vista_get[i] = field(s, s->base.object_get_class(vista), names[i],
                                        "System.Boolean");
                if (a->vista_get[i]) {
                    s->api.get(vista, a->vista_get[i], &a->vista_saved[i]);
                    a->vista_mask |= 1u << i;
                }
            }
        }
        if (di) {
            void *render_class = klass(s, "holo-game-interfaces.dll",
                                       "Niantic.Holoholo.Performance.Rendering",
                                       "IRenderQualityService");
            void *render_kind = render_class ? s->api.type_object(
                    s->base.class_get_type(render_class)) : NULL, *render = NULL;
            void *render_args[] = {render_kind};
            if (render_kind &&
                invoke(s, method(s, s->base.object_get_class(di), "TryResolve", NULL, 1, types), di,
                       render_args, &render) && render) {
                void *rk = s->base.object_get_class(render);
                const char *scale_args[] = {"System.Single"};
                float scale = 0;
                a->resolution_get = method(s, rk, "get_CurrentResolutionScale", "System.Single", 0,
                                           NULL);
                a->resolution_set = method(s, rk, "ApplyResolutionScale", "System.Void", 1,
                                           scale_args);
                if (a->resolution_get && a->resolution_set &&
                    scalar(s, a->resolution_get, render, &scale, 4) && isfinite(scale) &&
                    scale >= .25f && scale <= 2.f && keep(s, render, &a->render, false)) {
                    a->resolution_saved = scale;
                    a->resolution_applied = silph_raid_render_target(scale);
                    a->resolution_ready = 1;
                    __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                                        "light render scale saved=%.2f target=%.2f", scale,
                                        a->resolution_applied);
                }
            }
        }
        drop(s, &container);
        __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                            "light environment ready mask=%u error=%d", a->vista_mask, r);
    }
    void *render = NULL;
    float scale = 0;
    if (a->resolution_ready && target(s, a->render, &render) &&
        scalar(s, a->resolution_get, render, &scale, 4) && isfinite(scale)) {
        float next = restore ? a->resolution_saved : a->resolution_applied;
        bool apply = restore ? fabsf(scale - a->resolution_applied) < .001f : scale >
                                                                              a->resolution_applied +
                                                                              .001f;
        if (apply && fabsf(scale - next) > .001f) {
            void *args[] = {&next};
            if (invoke(s, a->resolution_set, render, args, NULL))
                __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                                    "render scale applied=%.2f restore=%u", next, restore);
        }
    }
    if (restore) {
        drop(s, &a->render);
        a->resolution_ready = 0;
    }
    if (!target(s, a->vista, &vista))return;
    for (unsigned i = 0; i < 3; i++)
        if (a->vista_mask & (1u << i)) {
            bool current = false;
            s->api.get(vista, a->vista_get[i], &current);
            bool value = restore ? a->vista_saved[i] : false;
            if ((!restore || !current) && current != value)
                s->api.set(vista, a->vista_get[i], &value);
        }
    if (restore)drop(s, &a->vista);
}

static void ar_graphics(Enhancer *s, uint64_t now, bool restore) {
    AutoRaid *a = &s->route;
    if (!(s->config.flags & SILPH_ENH_LIGHT_GRAPHICS))return;
    if (!restore && now < a->graphics_at)return;
    a->graphics_at = now + UINT64_C(5000000000);
    unsigned before = a->graphics_mask;
    ar_vista(s, restore);
    for (unsigned i = 0; i < 4; i++)
        if (a->graphics_get[i] && a->graphics_set[i]) {
            int32_t current = a->graphics_get[i]();
            if (current < (i == 3 ? -1 : 0) || current > 240)continue;
            if (restore) {
                if ((a->graphics_mask & (1u << i)) && current == a->graphics_applied[i])
                    a->graphics_set[i](a->graphics_saved[i]);
                continue;
            }
            int32_t applied = silph_raid_graphics_target(i, current);
            if (!(a->graphics_mask & (1u << i))) {
                a->graphics_saved[i] = current;
                a->graphics_mask |= 1u << i;
            }
            a->graphics_applied[i] = applied;
            if (current != applied)a->graphics_set[i](applied);
        }
    if (restore) {
        __android_log_print(ANDROID_LOG_INFO, "SilphRaid", "light graphics restored mask=%u",
                            a->graphics_mask);
        a->graphics_mask = 0;
    }
    else if (!before)
        __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                            "light graphics applied mask=%u saved=%d,%d,%d,%d fps=%d",
                            a->graphics_mask, a->graphics_saved[0], a->graphics_saved[1],
                            a->graphics_saved[2], a->graphics_saved[3], a->graphics_applied[3]);
}

static void ar_map_idle(Enhancer *s) {
    AutoRaid *a = &s->route;
    if (!a->map_tuned)return;
    int r = silph_rocket_map_settings(a->read, NULL, a->map_settings, a->map_saved, true);
    if (!r) {
        a->map_tuned = 0;
        __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                            "map refresh settings restored outside discovery");
    }
}

static int64_t ar_utc_ms(Enhancer *s) {
    return silph_raid_clock_now(&s->route.utc, s->route.site.server_ms, clock_ns());
}

static int64_t ar_lobby_remaining(Enhancer *s, void *gui, void *lobby, bool battle) {
    int64_t utc = ar_utc_ms(s);
    if (battle && utc >= 0) {
        int64_t end = 0;
        if (read_i64(s, lobby, "RaidBattleEndMs", &end)) {
            int64_t remaining = end - utc;
            if (remaining >= 0 && remaining <= 600000)return remaining;
        }
    }
    void *timer = NULL, *label = NULL, *text = NULL;
    char value[32] = {0};
    if (object_field(s, gui, "ekti", "Niantic.Holoholo.Battle.BattleTimerController", &timer) &&
        object_field(s, timer, "label", "UnityEngine.UI.Text", &label) &&
        read_ref(s, label, "get_text", &text) && label_string(s, text, value, sizeof(value))) {
        char *end = NULL;
        long seconds = strtol(value, &end, 10);
        if (end != value && !*end && seconds >= 0 && seconds <= 600)return seconds * 1000;
    }
    int64_t end = 0;
    if (!read_i64(s, lobby, battle ? "RaidBattleEndMs" : "PokemonSelectionEndMs", &end) ||
        end <= 0)
        return -1;
    if (utc < 0)return -1;
    int64_t remaining = end - utc;
    return remaining >= 0 && remaining <= 600000 ? remaining : -1;
}

static void ar_modal(Enhancer *s, void *gui, uint64_t now) {
    AutoRaid *a = &s->route;
    if (now < a->ui_at)return;
    void *title = NULL, *key = NULL;
    char text[96] = {0};
    if (!read_ref(s, gui, "get_TitleText", &title) || !read_ref(s, title, "get_TextKey", &key) ||
        !label_string(s, key, text, sizeof(text)))
        text[0] = 0;
    bool owned = ar_owns_gui(s);
    if (owned && !strcmp(text, "encounter_pokemon_box_full_title")) {
        ar_exit(s);
        ar_phase(s, 8, 0, 22, 0);
        return;
    }
    if (!owned && a->stage >= 1 && a->stage <= 3 && a->site.verified) {
        SilphRaidSite current = {0};
        owned = !silph_raid_read_current(a->reader, &current) && current.verified &&
                current.seed == a->site.seed && !strcmp(current.gym, a->site.gym);
    }
    const char *name = s->api.class_name(s->base.object_get_class(gui));
    bool large = name && !strcmp(name, "LargePlayerWarningModal");
    bool top = false;
    if (!scalar(s,
                method(s, s->base.object_get_class(gui), "get_IsTopGui", "System.Boolean", 0, NULL),
                gui, &top, 1) || !top)
        return;
    if (large) {
        bool open = false;
        if (owned && (a->stage == 2 || a->stage == 3 || a->stage == 9) && scalar(s, method(s,
                                                                                           s->base.object_get_class(
                                                                                                   gui),
                                                                                           "get_IsOpen",
                                                                                           "System.Boolean",
                                                                                           0, NULL),
                                                                                 gui, &open, 1) &&
            open && ar_void(s, gui, "Dismiss")) {
            a->ui_at = now + UINT64_C(500000000);
            __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                                "raid lobby courtesy warning dismissed stage=%u", a->stage);
        }
        return;
    }
    int32_t answer = (int32_t) silph_raid_modal_answer(text, owned, a->stage, a->ready != 0,
                                                       a->rejoining != 0);
    char body_key[96] = {0};
    const char *body_fields[] = {"horizontalMessage", "verticalMessage", "messageText"};
    for (unsigned i = 0; !answer && i < 3; i++) {
        void *body = NULL, *body_text = NULL;
        char value[96] = {0};
        if (object_field(s, gui, body_fields[i],
                         i == 2 ? "Niantic.Holoholo.UI.UIWidgets.AffluentText"
                                : "Niantic.Holoholo.UI.UIWidgets.HoloText", &body) &&
            read_ref(s, body, "get_TextKey", &body_text) &&
            label_string(s, body_text, value, sizeof(value))) {
            if (value[0])
                memcpy(body_key, value, sizeof(body_key));
            answer = (int32_t) silph_raid_modal_answer(value, owned, a->stage, a->ready != 0,
                                                       a->rejoining != 0);
            if (answer)memcpy(text, value, sizeof(text));
        }
    }
    if (!answer) {
        if (now >= a->modal_log_at) {
            a->modal_log_at = now + UINT64_C(5000000000);
            __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                                "raid modal pending key=%s body=%s stage=%u owned=%u", text,
                                body_key, a->stage, owned);
        }
        return;
    }
    void *args[] = {&answer};
    const char *types[] = {"Niantic.Holoholo.UI.ModalEvent"};
    if (invoke(s,
               method(s, s->base.object_get_class(gui), "ButtonPressed", "System.Void", 1, types),
               gui, args, NULL)) {
        a->ui_at = now + UINT64_C(150000000);
        __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                            "raid confirmation accepted gui=%s key=%s", name ? name : "", text);
    }
}

#include "autoraid-survey.inc"

static int ar_commands(Enhancer *s, uint64_t now) {
    AutoRaid *a = &s->route;
    for (unsigned i = 0; i < 4; i++) {
        SilphKecleonCommand c = {0};
        int r = silph_session_link_pop(a->link, &c);
        if (r == SILPH_SCHEDULE_BUSY || r == SILPH_SCHEDULE_STATE)return 0;
        if (r)return r;
        if (memcmp(&c.identity, &a->identity, sizeof(c.identity)))return SILPH_MANAGED_STALE;
        if (c.request != a->request)continue;
        if (c.kind == 1 && a->phase == 1) {
            unsigned species = 0, level = 0, form = 0, costume = 0, filters = 0, players = 0, wait = 0, evolution = 0;
            unsigned long long end = 0;
            int count = 0;
            if (sscanf(c.source, "%u:%u:%u:%u:%llu:%u:%u:%u:%u%n", &species, &level, &form,
                       &costume, &end, &filters, &players, &wait, &evolution, &count) != 9 ||
                (c.source[count] && c.source[count] != ':') || !species || species >= 2048 ||
                !level || level > 100 || filters > 63 || ((filters & 32) && species != 68) ||
                !players || players > 20 || wait < 5 || wait > 110 || evolution > 100)
                return SILPH_MANAGED_ARGUMENT;
            a->resume = c.source[count] == ':';
            a->resume_gym[0] = 0;
            if (a->resume) {
                const char *gym = c.source + count + 1;
                size_t size = strnlen(gym, 161);
                if (!size || size > 160)return SILPH_MANAGED_ARGUMENT;
                for (size_t j = 0; j < size; j++)
                    if (!((gym[j] >= 'a' && gym[j] <= 'z') || (gym[j] >= 'A' && gym[j] <= 'Z') ||
                          (gym[j] >= '0' && gym[j] <= '9') || gym[j] == '_' || gym[j] == '-' ||
                          gym[j] == '.'))
                        return SILPH_MANAGED_ARGUMENT;
                memcpy(a->resume_gym, gym, size + 1);
            }
            ar_survey_cancel(s);
            drop(s, &a->place);
            drop(s, &a->reward);
            drop(s, &a->recovery_pending);
            a->site = (SilphRaidSite) {0};
            a->utc = (SilphRaidClock) {0};
            a->species = species;
            a->level = level;
            a->form = form;
            a->costume = costume;
            a->evolution = evolution;
            a->expires = end;
            a->filters = filters & 31;
            a->group_cosmetics = !!(filters & 32);
            a->min_players = players;
            a->latitude = c.latitude;
            a->longitude = c.longitude;
            memcpy(a->source, c.source, sizeof(a->source));
            a->joined = a->ready = a->ticket_started = a->recommended = a->healing = a->won = a->nudged = a->desired_pass = a->lobby_gate = a->resolve_gate = a->reopen = a->rejoins = a->rejoining = a->leave_reason = a->recovery_care = 0;
            a->acted_gui = 0;
            a->expected = 0;
            a->map_at = a->lobby_at = 0;
            a->wait_seconds = wait;
            a->entry_gate = a->resolve_status = a->recovery_rpc = a->capture_authorized = 0;
            a->recovery_sync = 0;
            a->caught_before = s->metrics.catches;
            a->fled_before = s->metrics.fled;
            ar_phase(s, 2, 0, 0, now + UINT64_C(15000000000));
        } else if (c.kind == 6 && a->phase == 1) { ar_survey_request(s, now); }
        else if (c.kind == 2 && (a->phase == 2 || a->phase == 6) &&
                 c.move == (a->phase == 6 ? 1u : 0u)) {
            if (!c.accepted)
                ar_done(s, 2);
            else {
                a->scan = 0;
                a->nudge_at = now + UINT64_C(4000000000);
                ar_phase(s, 3, 0, 0, now + UINT64_C(25000000000));
            }
        }
        else if (c.kind == 3 && a->phase == 4 && a->stage == 6 && c.accepted) {
            double latitude = 0, longitude = 0;
            if (silph_game_read_rocket_position(a->game, &latitude, &longitude))continue;
            double dy = latitude - a->capture_latitude, dx =
                    (longitude - a->capture_longitude) * cos(latitude * .017453292519943295);
            if (dx * dx + dy * dy > 0.0000000081) {
                a->capture_latitude = latitude;
                a->capture_longitude = longitude;
                a->dirty = 1;
                continue;
            }
            a->capture_authorized = 1;
            ar_phase(s, 5, 6, 0, now + UINT64_C(180000000000));
            a->ui_at = now;
        } else if (c.kind == 4 && a->phase == 7) {
            a->won = 0;
            a->expected = 0;
            a->request++;
            silph_game_read_position(a->game, &a->latitude, &a->longitude);
            ar_phase(s, 1, 0, 0, 0);
        }
        else if (c.kind == 5 && a->phase == 1)ar_phase(s, 8, 0, 6, 0);
    }
    return 0;
}

static bool ar_matching(Enhancer *s, const SilphRaidSite *site) {
    AutoRaid *a = &s->route;
    double dy = site->latitude - a->latitude, dx =
            (site->longitude - a->longitude) * cos(a->latitude * .017453292519943295);
    bool active = site->active || site->complete;
    bool same = !a->resume ||
                (!strcmp(site->gym, a->resume_gym) && site->end_ms / 1000 == a->expires / 1000);
    return dx * dx + dy * dy < 0.0000000052 && active && same && site->species == a->species &&
           site->level == a->level && ((a->species == 68 && a->group_cosmetics) ||
                                       ((!a->form || site->form == a->form) &&
                                        site->costume == a->costume)) &&
           (!a->evolution || (site->evolution_known && site->evolution == a->evolution));
}

static int ar_scan(Enhancer *s, uint64_t now) {
    AutoRaid *a = &s->route;
    if (!a->scan) {
        int r = silph_raid_read_begin(a->reader);
        if (r)return r;
        a->scan = 1;
        a->scan_best_m = INFINITY;
        a->scan_best = (SilphRaidSite) {0};
    }
    uint64_t began = clock_ns();
    for (unsigned i = 0; i < 4 && clock_ns() - began < UINT64_C(1500000); i++) {
        SilphRaidSite site = {0};
        bool finished = false;
        int r = silph_raid_read_next(a->reader, &site, &finished);
        if (r == SILPH_MANAGED_EXCEPTION) {
            a->scan = 0;
            if (now >= a->deadline)ar_done(s, 3);
            return 0;
        }
        if (r)return r;
        if (finished) {
            a->scan = 0;
            if (now >= a->scan_log_at) {
                double lat = 0, lon = 0;
                int pos = silph_game_read_rocket_position(a->game, &lat, &lon);
                double dy = lat - a->latitude, dx =
                        (lon - a->longitude) * cos(a->latitude * .017453292519943295);
                SilphRaidSite *v = &a->scan_best;
                __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                                    "candidate check nearestM=%.1f playerM=%.1f positionError=%d species=%u/%u level=%u/%u form=%u/%u costume=%u/%u evolution=%u/%u verified=%u active=%u complete=%u",
                                    a->scan_best_m, hypot(dx, dy) * 111195, pos, v->species,
                                    a->species, v->level, a->level, v->form, a->form, v->costume,
                                    a->costume, v->evolution, a->evolution, v->verified, v->active,
                                    v->complete);
                a->scan_log_at = now + UINT64_C(3000000000);
            }
            if (now >= a->deadline)ar_done(s, 3);
            return 0;
        }
        double sy = site.latitude - a->latitude, sx =
                (site.longitude - a->longitude) * cos(a->latitude * .017453292519943295), distance =
                hypot(sx, sy) * 111195;
        if (distance < a->scan_best_m) {
            a->scan_best_m = distance;
            a->scan_best = site;
        }
        if (!ar_matching(s, &site))continue;
        double player_lat = 0, player_lon = 0;
        if (silph_game_read_rocket_position(a->game, &player_lat, &player_lon))return 0;
        double dy = player_lat - a->latitude, dx =
                (player_lon - a->longitude) * cos(a->latitude * .017453292519943295);
        if (dx * dx + dy * dy > 0.00000016)return 0;
        SilphManagedRef gym = {0};
        void *object = NULL;
        r = silph_raid_read_gym(a->reader, site.gym, &gym);
        if (!r)r = silph_managed_target(a->managed, gym, &object);
        if (!r && !keep(s, object, &a->place, false))r = SILPH_MANAGED_CAPACITY;
        if (gym.serial)silph_managed_release(a->managed, gym);
        if (r)return r;
        a->site = site;
        void *explore = NULL, *root = NULL;
        if (!target(s, a->explore, &explore) || !target(s, a->root, &root))
            return SILPH_MANAGED_NOT_READY;
        ar_void(s, root, "PlayerSawRaidIntroSequence");
        bool remote = false;
        int32_t context = 0;
        const char *types[] = {"Niantic.Holoholo.Gym.MapGym", "System.Boolean", "System.Action",
                               "Niantic.Holoholo.Gym.PreviousContext"};
        void *args[] = {object, &remote, NULL, &context};
        const void *open = method(s, s->base.object_get_class(explore), "GymSelected",
                                  "System.Void", 4, types);
        if (!open) {
            __android_log_print(ANDROID_LOG_ERROR, "SilphRaid",
                                "GymSelected signature unavailable");
            return SILPH_MANAGED_CONTRACT;
        }
        if (!invoke(s, open, explore, args, NULL)) {
            ar_phase(s, 5, 1, 0, now);
            ar_done(s, 7);
            return 0;
        }
        a->ui_at = now;
        ar_phase(s, 5, 1, 0, now + UINT64_C(600000000000));
        return 0;
    }
    return 0;
}

static bool ar_cost(Enhancer *s, void *gui, uint32_t wanted) {
    void *cost = NULL, *items = NULL;
    if (!object_field(s, gui, "eeiw", "Holoholo.Rpc.RaidEntryCostProto", &cost))return false;
    const void *list = s->api.field(s->base.object_get_class(cost), "ItemRequirement");
    if (!list)return false;
    s->api.get(cost, list, &items);
    if (!items)return false;
    int32_t remote = 0, count = 0;
    if (!read_i32(s, cost, "RaidType", &remote, true) || remote == 2 ||
        !scalar(s, method(s, s->base.object_get_class(items), "get_Count", "System.Int32", 0, NULL),
                items, &count, 4))
        return false;
    if (count != 1) {
        __android_log_print(ANDROID_LOG_INFO, "SilphRaid", "entry cost requirements=%d location=%d",
                            count, remote);
        return false;
    }
    int32_t index = 0, item = 0, n = 0;
    void *args[] = {&index}, *entry = NULL;
    const char *types[] = {"System.Int32"};
    bool valid =
            invoke(s, method(s, s->base.object_get_class(items), "get_Item", NULL, 1, types), items,
                   args, &entry) && read_i32(s, entry, "Item", &item, true) &&
            read_i32(s, entry, "Count", &n, false);
    if (valid)
        __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                            "entry cost item=%d count=%d location=%d wanted=%u", item, n, remote,
                            wanted);
    return valid && item == (int32_t) wanted && n == 1;
}

static int ar_approach(Enhancer *s, const SilphSchedulePermit *permit, void *gui, uint64_t now) {
    AutoRaid *a = &s->route;
    SilphRaidSite current = {0};
    int r = silph_raid_read_current(a->reader, &current);
    if (r)return r;
    unsigned gate = !current.verified ? 1 : strcmp(current.gym, a->site.gym) ? 2 : current.seed !=
                                                                                   a->site.seed ? 3
                                                                                                : !current.pass_known
                                                                                                  ? 4
                                                                                                  : !current.full_details
                                                                                                    ? 5
                                                                                                    : 6;
    if (gate != a->entry_gate) {
        a->entry_gate = gate;
        __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                            "entry details gate=%u seed=%llu expected=%llu full=%u result=%u complete=%u paid=%u join=%u throws=%d",
                            gate, (unsigned long long) current.seed,
                            (unsigned long long) a->site.seed, current.full_details,
                            current.details_result, current.complete, current.ticket_used,
                            current.can_join, current.throws_remaining);
    }
    if (gate < 5)return 0;
    if (current.full_details && current.complete) {
        if (current.rewards_claimed && current.flushed && current.throws_remaining == 0) {
            if (a->resume)a->won = 1;
            __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                                "raid already completed; advancing without a ticket");
            ar_done(s, a->resume ? 0 : 5);
            return 0;
        }
        if (current.ticket_used) {
            a->resume = 1;
            a->won = 1;
            a->dirty = 1;
        }
        else {
            ar_done(s, 5);
            return 0;
        }
    }
    if (a->resume) {
        if (!current.full_details)return 0;
        if (current.rewards_claimed && current.flushed && current.throws_remaining == 0) {
            ar_done(s, 5);
            return 0;
        }
        if (!current.ticket_used || !current.complete) {
            ar_phase(s, 8, 0, 10, 0);
            return 0;
        }
        if (!a->joined) {
            if (!ar_void(s, gui, "OnJoinButtonPressed")) {
                ar_done(s, 7);
                return 0;
            }
            a->joined = 1;
        }
        return 0;
    }
    if (!current.in_range) {
        if (now - a->ui_at > UINT64_C(8000000000))ar_done(s, 2);
        return 0;
    }
    if (!current.can_join && !current.ticket_used) {
        ar_done(s, 1);
        return 0;
    }
    uint32_t free = 0, premium = 0;
    r = silph_game_read_item(a->game, 1401, &free);
    if (!r)r = silph_game_read_item(a->game, 1402, &premium);
    if (r)return r;
    if (!free && current.free_available && !current.ticket_used) {
        SilphRaidPassMetrics result = {0};
        r = silph_raid_pass_poll(a->pass, now, &result);
        if (r || result.uncertain)return SILPH_MANAGED_STALE;
        if (result.pending)return 0;
        bool same = result.raid_seed == a->site.seed && !strcmp(result.gym, a->site.gym);
        if (!same) {
            r = silph_raid_pass_request(a->pass, permit, a->site.gym, a->site.seed, now);
            return r == SILPH_MANAGED_NOT_READY ? 0 : r;
        }
        if (result.result == 1)return 0;
        if (result.result == 4) {
            ar_done(s, 2);
            return 0;
        }
        if (result.result != 3 && result.result != 5)return SILPH_MANAGED_NOT_READY;
    }
    if (!free && !premium && !current.ticket_used) {
        ar_phase(s, 8, 0, 8, 0);
        return 0;
    }
    if (!a->joined) {
        a->desired_pass = free ? 1401 : premium ? 1402 : 0;
        __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                            "entry inventory free=%u premium=%u chosen=%u paid=%u", free, premium,
                            a->desired_pass, current.ticket_used);
        if (!ar_void(s, gui, "OnJoinButtonPressed")) {
            ar_done(s, 7);
            return 0;
        }
        a->joined = 1;
        a->ui_at = now;
        ar_phase(s, 5, 2, 0, a->deadline);
    }
    return 0;
}

static bool ar_party(Enhancer *s, void *lobby, void **panel) {
    void *prep = NULL, *scroll = NULL;
    return object_field(s, lobby, "ektl", "Niantic.Holoholo.Raid.RaidBattlePrepUiController",
                        &prep) && object_field(s, prep, "dynoScrollRect",
                                               "Niantic.Holoholo.Gym.BattlePrepPartyDynoScrollRect",
                                               &scroll) &&
           read_ref(s, scroll, "CurrentPageGui", panel);
}

static void ar_gate(Enhancer *s, unsigned gate) {
    if (s->route.lobby_gate != gate) {
        s->route.lobby_gate = gate;
        __android_log_print(ANDROID_LOG_INFO, "SilphRaid", "lobby gate=%u", gate);
    }
}

static bool ar_team_exhausted(Enhancer *s) {
    void *raid = NULL, *director = NULL, *state = NULL, *actor = NULL;
    bool available = true;
    if (!s->raid.serial || !target(s, s->raid, &raid))return false;
    s->api.get(raid, s->raid_director, &director);
    if (!director || !read_ref(s, director, "get_CurrentState", &state) ||
        !read_ref(s, state, "get_LocalActor", &actor) || !scalar(s, method(s,
                                                                           s->base.object_get_class(
                                                                                   actor),
                                                                           "get_HasAvailablePokemon",
                                                                           "System.Boolean", 0,
                                                                           NULL), actor, &available,
                                                                 1) || available)
        return false;
    void *roster = NULL, *iterator = NULL;
    if (!read_ref(s, actor, "get_RosterList", &roster) ||
        !read_ref(s, roster, "GetEnumerator", &iterator))
        return false;
    uint64_t ids[42] = {0}, team[6] = {0};
    unsigned count = 0, selected = 0;
    void *k = s->base.object_get_class(iterator);
    const void *next = method(s, k, "MoveNext", "System.Boolean", 0, NULL), *current = method(s, k,
                                                                                              "get_Current",
                                                                                              NULL,
                                                                                              0,
                                                                                              NULL);
    for (unsigned i = 0; i < 43; i++) {
        bool more = false;
        void *member = NULL, *pokemon = NULL;
        if (!scalar(s, next, iterator, &more, 1))return false;
        if (!more)break;
        if (i == 42 || !invoke(s, current, iterator, NULL, &member) ||
            !read_ref(s, member, "get_Pokemon", &pokemon))
            return false;
        const void *id = field(s, s->base.object_get_class(pokemon), "Id", "System.UInt64");
        if (!id)return false;
        s->api.get(pokemon, id, &ids[count]);
        if (!ids[count])return false;
        count++;
    }
    if (count == 6) {
        memcpy(team, ids, sizeof(team));
        selected = 6;
    }
    else
        for (unsigned i = 0; i < count; i++)
            for (unsigned j = 0; j < 6; j++)
                if (ids[i] == s->route.team[j]) {
                    if (selected == 6)return false;
                    team[selected++] = ids[i];
                    break;
                }
    if (selected != 6)return false;
    unsigned changed = 0;
    for (unsigned i = 0; i < 6; i++) {
        if (team[i] != s->route.team[i])changed++;
        for (unsigned j = 0; j < i; j++)if (team[j] == team[i])return false;
    }
    memcpy(s->route.team, team, sizeof(team));
    __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                        "team recovery roster synchronized members=6 changed=%u", changed);
    return true;
}

static void ar_recover_team(Enhancer *s) {
    AutoRaid *a = &s->route;
    if (a->rejoins >= 6) {
        ar_done(s, 9);
        return;
    }
    if (!s->care_pending.serial) {
        s->care_ready = s->care_count = s->care_index = 0;
        a->medicine_active = a->medicine_mask = a->medicine_wait = 0;
        drop(s, &a->medicine_list);
    }
    drop(s, &a->recovery_pending);
    a->recovery_rpc = 0;
    a->recommended = a->healing = a->ready = a->rejoining = 0;
    a->recovery_care = 1;
    a->recovery_sync = a->rejoin_deadline = 0;
    a->acted_gui = 0;
    ar_phase(s, 5, 9, 0, a->deadline);
    uint32_t free = 0, premium = 0;
    int inventory = silph_game_read_item(a->game, 1401, &free);
    if (!inventory)inventory = silph_game_read_item(a->game, 1402, &premium);
    __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                        "team recovery started rejoin=%u seed=%llu exhausted=1 free=%u premium=%u inventory=%d",
                        a->rejoins + 1, (unsigned long long) a->site.seed, free, premium,
                        inventory);
}

static int
ar_sync_recovery(Enhancer *s, const SilphSchedulePermit *permit, void *gui, uint64_t now) {
    AutoRaid *a = &s->route;
    if (a->recovery_pending.serial) {
        void *promise = NULL;
        bool complete = false, failed = false;
        bool valid = target(s, a->recovery_pending, &promise);
        void *k = valid ? s->base.object_get_class(promise) : NULL;
        const void *done = valid ? field(s, k, "completeCalled", "System.Boolean")
                                 : NULL, *error = valid ? field(s, k, "errorCalled",
                                                                "System.Boolean") : NULL;
        if (done && error) {
            s->api.get(promise, done, &complete);
            s->api.get(promise, error, &failed);
        }
        else valid = false;
        if (valid && !complete && !failed && now < a->recovery_sync)return 0;
        drop(s, &a->recovery_pending);
        if (!valid || !complete || failed) {
            __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                                "team recovery synchronization failed step=%u", a->recovery_rpc);
            return -1;
        }
        a->recovery_rpc++;
        a->recovery_sync = 0;
        __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                            "team recovery synchronization confirmed step=%u", a->recovery_rpc);
        return a->recovery_rpc == 4 ? 1 : 0;
    }
    if (a->recovery_rpc == 4)return 1;
    if (a->recovery_rpc == 2)a->recovery_rpc = 3;
    if (!silph_schedule_permitted(permit, true))return 0;
    void *service = NULL, *promise = NULL;
    bool ok = false;
    if (a->recovery_rpc == 0) {
        ok = object_field(s, gui, "eksq", "Niantic.Holoholo.VNext.IRaidVNextRpcService",
                          &service) && invoke(s, method(s, s->base.object_get_class(service),
                                                        "SyncVNextBattle", NULL, 0, NULL), service,
                                              NULL, &promise);
    }
    else if (a->recovery_rpc == 3) {
        SilphManagedRef container = {0};
        void *di = NULL, *kind = klass(s, "holo-game-interfaces.dll",
                                       "Niantic.Holoholo.Collections", "IInventoryCache");
        void *type_object = kind ? s->api.type_object(s->base.class_get_type(kind)) : NULL;
        const char *types[] = {"System.Type"};
        void *args[] = {type_object};
        int r = ar_service(s, 5, &container);
        ok = !r && type_object && target(s, container, &di) &&
             invoke(s, method(s, s->base.object_get_class(di), "TryResolve", NULL, 1, types), di,
                    args, &service) && service && invoke(s, method(s, s->base.object_get_class(
                service), "UpdateInventory", NULL, 0, NULL), service, NULL, &promise);
        drop(s, &container);
    }
    if (!ok || !promise || !keep(s, promise, &a->recovery_pending, false)) {
        __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                            "team recovery synchronization unavailable step=%u", a->recovery_rpc);
        return -1;
    }
    if (a->recovery_rpc == 0)a->recovery_rpc = 1;
    a->recovery_sync = now + UINT64_C(15000000000);
    __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                        "team recovery synchronization requested step=%u", a->recovery_rpc);
    return 0;
}

static int ar_lobby(Enhancer *s, const SilphSchedulePermit *permit, void *gui, uint64_t now) {
    AutoRaid *a = &s->route;
    void *details = NULL, *lobby = NULL, *panel = NULL;
    if (!ar_owns_gui(s) ||
        !object_field(s, gui, "ekty", "Holoholo.Rpc.RaidDetailsContainer", &details) ||
        !object_field(s, gui, "ektx", "Holoholo.Rpc.LobbyProto", &lobby))
        return 0;
    int64_t seed = 0;
    if (!scalar(s, method(s, s->base.object_get_class(details), "get_RaidSeed", "System.Int64", 0,
                          NULL), details, &seed, 8) || (uint64_t) seed != a->site.seed) {
        ar_gate(s, 10);
        return 0;
    }
    bool started = false;
    if (!scalar(s,
                method(s, s->base.object_get_class(gui), "get_BattleStarted", "System.Boolean", 0,
                       NULL), gui, &started, 1))
        return 0;
    if (started && a->stage != 9) {
        if (a->stage != 4) {
            int64_t end = 0, utc = ar_utc_ms(s);
            if (utc >= 0 && read_i64(s, lobby, "RaidBattleEndMs", &end) && end > 0 && end <= utc) {
                __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                                    "paid lobby battle already ended; no rejoin requested");
                ar_done(s, 9);
            }
            return 0;
        }
        if (s->raid.serial && !ar_team_exhausted(s))return 0;
        ar_recover_team(s);
        if (a->stage != 9)return 0;
    }
    if (!started && a->stage == 9)return 0;
    int64_t remaining = ar_lobby_remaining(s, gui, lobby, started);
    int32_t players = 0;
    bool count_known =
            read_i32(s, lobby, "TotalPlayerCountInLobby", &players, false) && players >= 0;
    bool enough = count_known && players >= (int32_t) a->min_players, team_ready =
            a->healing && !s->care_ready && !s->care_pending.serial;
    if ((started && remaining >= 0 && remaining <= 3000) || (!started && count_known &&
                                                             silph_raid_lobby_leave(remaining,
                                                                                    a->wait_seconds,
                                                                                    (uint32_t) players,
                                                                                    a->min_players,
                                                                                    true))) {
        if (!ar_void(s, gui, "LeaveLobby"))return SILPH_MANAGED_EXCEPTION;
        __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                            "lobby left players=%d required=%u remainingMs=%lld teamReady=%u battle=%u",
                            players, a->min_players, (long long) remaining, team_ready, started);
        ar_done(s, enough ? 9 : 1);
        return 0;
    }
    if (a->ready && !started)return 0;
    if (a->recovery_care == 1) {
        if (s->care_pending.serial || s->care_ready) {
            care_tick(s, a->read, a->game, a->managed, permit, now, true);
            return 0;
        }
        int sync = ar_sync_recovery(s, permit, gui, now);
        if (sync < 0) {
            ar_done(s, 9);
            return 0;
        }
        if (!sync)return 0;
        bool synced = true;
        unsigned fainted = 0, hurt = 0, healthy = 0;
        for (unsigned i = 0; i < 6; i++) {
            SilphManagedRef owned = {0};
            SilphReadValue hp = {0}, maximum = {0}, id = {0};
            int r = silph_game_read_owned(a->game, a->team[i], &owned);
            if (!r && owned.serial)
                r = silph_read_field(a->read, owned, "stamina_", SILPH_READ_I32, &hp);
            else r = SILPH_MANAGED_NOT_READY;
            if (!r)r = silph_read_field(a->read, owned, "maxStamina_", SILPH_READ_I32, &maximum);
            if (!r)r = silph_read_field(a->read, owned, "id_", SILPH_READ_U64, &id);
            if (owned.serial)silph_managed_release(a->managed, owned);
            if (r || id.bits != a->team[i] || !maximum.bits || hp.bits > maximum.bits)
                synced = false;
            else if (!hp.bits)fainted++; else if (hp.bits < maximum.bits)hurt++; else healthy++;
        }
        if (!synced) {
            if (!a->recovery_sync)a->recovery_sync = now + UINT64_C(8000000000);
            if (now >= a->recovery_sync) {
                __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                                    "team recovery inventory not synchronized; no medicine requested");
                ar_done(s, 9);
            }
            return 0;
        }
        __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                            "team recovery inventory synchronized fainted=%u hurt=%u healthy=%u",
                            fainted, hurt, healthy);
        memcpy(s->care_team, a->team, sizeof(a->team));
        s->care_count = 6;
        s->care_index = 0;
        s->care_expected = 0;
        s->care_next = now;
        s->care_ready = 1;
        a->recovery_care = 2;
    }
    if (a->recovery_care == 2) {
        care_tick(s, a->read, a->game, a->managed, permit, now, true);
        if (s->care_pending.serial || s->care_ready)return 0;
        a->recovery_care = 0;
    }
    if (!started)ar_phase(s, 5, 3, 0, a->deadline);
    if (!ar_party(s, gui, &panel)) {
        ar_gate(s, 1);
        return 0;
    }
    if (!a->recommended) {
        if (!started && a->recovery_rpc != 4) {
            int sync = ar_sync_recovery(s, permit, gui, now);
            if (sync < 0) {
                a->recovery_rpc = 4;
                __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                                    "initial inventory synchronization unavailable; continuing team health checks");
            }
            else if (!sync) {
                if (remaining >= 0 && remaining <= 1000)ar_done(s, 9);
                return 0;
            }
        }
        ar_void(s, panel, "RerollParty");
        a->recommended = 1;
        return 0;
    }
    if (!a->healing && (s->care_pending.serial || s->care_ready)) {
        care_tick(s, a->read, a->game, a->managed, permit, now, true);
        return 0;
    }
    if (!a->healing) {
        void *pokemon = NULL, *iterator = NULL;
        if (!read_ref(s, panel, "get_PokemonProtos", &pokemon) ||
            !read_ref(s, pokemon, "GetEnumerator", &iterator)) {
            ar_gate(s, 2);
            return 0;
        }
        uint64_t ids[6] = {0};
        for (int32_t i = 0; i < 6; i++) {
            void *member = NULL;
            bool more = false;
            if (!scalar(s,
                        method(s, s->base.object_get_class(iterator), "MoveNext", "System.Boolean",
                               0, NULL), iterator, &more, 1) || !more ||
                !read_ref(s, iterator, "get_Current", &member)) {
                ar_gate(s, 3);
                return 0;
            }
            const void *id_field = field(s, s->base.object_get_class(member), "id_",
                                         "System.UInt64");
            if (id_field)s->api.get(member, id_field, &ids[i]);
            if (!ids[i]) {
                ar_gate(s, 3);
                return 0;
            }
        }
        memcpy(a->team, ids, sizeof(ids));
        bool complete = true;
        for (unsigned i = 0; i < 6; i++) {
            SilphManagedRef owned = {0};
            SilphReadValue hp = {0}, max = {0};
            int r = silph_game_read_owned(a->game, ids[i], &owned);
            if (!r && owned.serial)
                r = silph_read_field(a->read, owned, "stamina_", SILPH_READ_I32, &hp);
            else r = SILPH_MANAGED_NOT_READY;
            if (!r)r = silph_read_field(a->read, owned, "maxStamina_", SILPH_READ_I32, &max);
            if (owned.serial)silph_managed_release(a->managed, owned);
            if (r || !max.bits || hp.bits < max.bits)complete = false;
        }
        a->healing = 1;
        s->care_expected = 0;
        s->care_index = 0;
        if (!complete) {
            memcpy(s->care_team, ids, sizeof(ids));
            s->care_count = 6;
            s->care_ready = 1;
            s->care_next = now;
        }
        else { s->care_count = s->care_ready = 0; }
        ar_gate(s, 4);
    }
    if (s->care_ready || s->care_pending.serial) {
        care_tick(s, a->read, a->game, a->managed, permit, now, true);
        if (!started && remaining >= 0 && remaining <= 1000 &&
            (s->care_ready || s->care_pending.serial)) {
            if (!ar_void(s, gui, "LeaveLobby"))
                return SILPH_MANAGED_EXCEPTION;
            ar_done(s, 9);
        }
        return 0;
    }
    bool usable = true;
    for (unsigned i = 0; i < 6; i++) {
        SilphManagedRef owned = {0};
        SilphReadValue health = {0};
        int r = silph_game_read_owned(a->game, a->team[i], &owned);
        if (!r && owned.serial)
            r = silph_read_field(a->read, owned, "stamina_", SILPH_READ_I32, &health);
        else r = SILPH_MANAGED_NOT_READY;
        if (owned.serial)silph_managed_release(a->managed, owned);
        if (r) {
            ar_gate(s, 8);
            return 0;
        }
        usable = usable && (int32_t) health.bits > 0;
    }
    if (!usable) {
        ar_gate(s, 9);
        if (!ar_void(s, gui, "LeaveLobby"))return SILPH_MANAGED_EXCEPTION;
        ar_done(s, 9);
        return 0;
    }
    if (started) {
        if (!a->rejoining && now >= a->ui_at) {
            if (!ar_void(s, gui, "RejoinBattle"))return SILPH_MANAGED_EXCEPTION;
            a->rejoining = 1;
            a->battle_ref = s->raid.serial;
            a->rejoins++;
            a->rejoin_deadline = now + UINT64_C(15000000000);
            a->ui_at = now;
            uint32_t free = 0, premium = 0;
            int inventory = silph_game_read_item(a->game, 1401, &free);
            if (!inventory)inventory = silph_game_read_item(a->game, 1402, &premium);
            __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                                "same battle rejoin requested attempt=%u seed=%llu free=%u premium=%u inventory=%d; no ticket requested",
                                a->rejoins, (unsigned long long) a->site.seed, free, premium,
                                inventory);
        } else if (a->rejoining && now >= a->rejoin_deadline) { ar_done(s, 9); }
        return 0;
    }
    if (!enough) {
        ar_gate(s, 5);
        return 0;
    }
    if (!a->ready) {
        void *button = NULL;
        bool enabled = false;
        bool known = read_ref(s, panel, "get_SubmitButton", &button) && scalar(s, method(s,
                                                                                         s->base.object_get_class(
                                                                                                 button),
                                                                                         "get_Interactable",
                                                                                         "System.Boolean",
                                                                                         0, NULL),
                                                                               button, &enabled, 1);
        if (!known)
            known = object_field(s, panel, "readyButton", "UnityEngine.UI.Button", &button) &&
                    scalar(s, method(s, s->base.object_get_class(button), "get_interactable",
                                     "System.Boolean", 0, NULL), button, &enabled, 1);
        if (known && enabled) {
            if (ar_void(s, panel, "OnReadyClicked")) {
                a->ready = 1;
                ar_gate(s, 7);
                __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                                    "lobby ready players=%d remainingMs=%lld", players,
                                    (long long) remaining);
            }
        }
        else ar_gate(s, 6);
    }
    return 0;
}

static int ar_accept_reward(Enhancer *s, void *reward) {
    AutoRaid *a = &s->route;
    void *pokemon = NULL;
    int32_t iv[3] = {0};
    bool shiny = false;
    if (!object_field(s, reward, "Pokemon", "Holoholo.Rpc.PokemonProto", &pokemon))return -1;
    if (!a->filters)return 1;
    const char *names[] = {"get_IndividualAttack", "get_IndividualDefense",
                           "get_IndividualStamina"};
    for (unsigned i = 0; i < 3; i++)
        if (!scalar(s, method(s, s->base.object_get_class(pokemon), names[i], "System.Int32", 0,
                              NULL), pokemon, &iv[i], 4) || iv[i] < 0 || iv[i] > 15)
            return -1;
    uint32_t known = SILPH_RAID_KNOWN_IV;
    bool background = false;
    void *display = NULL;
    if (read_ref(s, pokemon, "get_PokemonDisplay", &display)) {
        const void *f = field(s, s->base.object_get_class(display), "Shiny", "System.Boolean");
        if (f) {
            s->api.get(display, f, &shiny);
            known |= SILPH_RAID_KNOWN_SHINY;
        }
        if (a->filters & (SILPH_RAID_CATCH_BACKGROUND | SILPH_RAID_CATCH_SHINY_BACKGROUND)) {
            bool card = false, art = false;
            void *k = s->base.object_get_class(display);
            if (scalar(s, method(s, k, "get_HasLocationCard", "System.Boolean", 0, NULL), display,
                       &card, 1) &&
                scalar(s, method(s, k, "get_IsNaturalArtPokemon", "System.Boolean", 0, NULL),
                       display, &art, 1)) {
                known |= SILPH_RAID_KNOWN_BACKGROUND;
                background = card || art;
            }
        }
    }
    return silph_raid_catch_accept(a->filters, known, iv[0], iv[1], iv[2], shiny, background);
}

static void ar_resolve_gate(Enhancer *s, unsigned gate) {
    if (s->route.resolve_gate != gate) {
        s->route.resolve_gate = gate;
        __android_log_print(ANDROID_LOG_INFO, "SilphRaid", "resolve gate=%u", gate);
    }
}

#include "autoraid-capture.inc"

static int ar_resolve(Enhancer *s, void *gui, uint64_t now) {
    AutoRaid *a = &s->route;
    void *info = NULL, *root = NULL, *fort = NULL;
    char gym[161] = {0};
    int32_t status = 0;
    if (!target(s, a->root, &root) || !read_ref(s, root, "get_FortId", &fort) ||
        !label_string(s, fort, gym, sizeof(gym)) || strcmp(gym, a->site.gym)) {
        ar_resolve_gate(s, 6);
        return 0;
    }
    if (!object_field(s, gui, "elcx", "Niantic.Holoholo.Raid.IRaidResolveInformation", &info) ||
        !read_i32(s, info, "<Status>k__BackingField", &status, true)) {
        ar_resolve_gate(s, 5);
        return 0;
    }
    if (status >= 0 && a->resolve_status != (uint32_t) status + 1) {
        a->resolve_status = (uint32_t) status + 1;
        uint32_t free = 0, premium = 0;
        int inventory = silph_game_read_item(a->game, 1401, &free);
        if (!inventory)inventory = silph_game_read_item(a->game, 1402, &premium);
        __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                            "resolve status=%d seed=%llu free=%u premium=%u inventory=%d", status,
                            (unsigned long long) a->site.seed, free, premium, inventory);
    }
    if (status == 5 && a->stage == 9 && a->recovery_care) {
        if (a->acted_gui != (uintptr_t) gui && ar_void(s, gui, "Close")) {
            a->acted_gui = (uintptr_t) gui;
            __android_log_print(ANDROID_LOG_INFO, "SilphRaid", "exhausted team notice dismissed");
        }
        return 0;
    }
    if (status == 3 || status == 4) {
        ar_void(s, gui, "Close");
        ar_done(s, 9);
        return 0;
    }
    if (status != 2)return 0;
    if (!a->won) {
        a->won = 1;
        a->dirty = 1;
        if (!a->resume)s->metrics.raids++;
    }
    return ar_prepare_capture(s, gui, info, now);
}

static int ar_visual_capture(Enhancer *s, uint64_t now) {
    AutoRaid *a = &s->route;
    if (now < a->ui_at)return 0;
    void *interaction = NULL, *state = NULL, *map = NULL, *reward = NULL, *fort = NULL;
    char gym[161] = {0};
    if (!target(s, s->interaction, &interaction) || !target(s, s->state, &state) ||
        !invoke(s, s->state_map, state, NULL, &map) || !map) {
        ar_resolve_gate(s, 20);
        return 0;
    }
    if (!exact(s, map, a->map_pokemon_class) ||
        !object_field(s, map, "eljb", "Holoholo.Rpc.RaidEncounterProto", &reward) ||
        !object_field(s, reward, "FortId", "System.String", &fort) ||
        !label_string(s, fort, gym, sizeof(gym)) || strcmp(gym, a->site.gym)) {
        ar_resolve_gate(s, 21);
        return 0;
    }
    int64_t id = 0;
    if (!read_i64(s, reward, "EncounterId", &id) || !id || (uint64_t) id != s->last_id) {
        ar_resolve_gate(s, 22);
        return 0;
    }
    a->expected = (uint64_t) id;
    if (s->promise.serial || s->berry_promise.serial)return 0;
    int accepted = ar_accept_reward(s, reward);
    if (accepted < 0) {
        ar_resolve_gate(s, 4);
        return 0;
    }
    if (!accepted) {
        int32_t result = 1;
        void *args[] = {&result};
        if (!invoke(s, s->state_complete, state, args, NULL))return SILPH_MANAGED_EXCEPTION;
        ar_phase(s, 5, 7, 4, a->deadline);
        return 0;
    }
    if (!ar_capture_position(s, now))return 0;
    if ((s->metrics.available & SILPH_CFG_BERRIES) && !s->berry_tried) {
        bool can = false;
        int32_t active = 0;
        if (scalar(s, s->can_berry, interaction, &can, 1) && can &&
            scalar(s, s->active_berry, interaction, &active, 4) && !active)
            return 0;
    }
    void *ball = NULL;
    int32_t item = 0;
    if (!invoke(s, s->interaction_ball, interaction, NULL, &ball) || !ball ||
        !scalar(s, s->ball_type, ball, &item, 4) ||
        (item != 5 && item != 6 && item != 7 && item != 8)) {
        ar_resolve_gate(s, 23);
        return 0;
    }
    bool playing = true;
    if (!scalar(s, s->ball_in_play, interaction, &playing, 1) || playing) {
        ar_resolve_gate(s, 24);
        return 0;
    }
    const float sizes[] = {0, 1.1f, .85f, .5f, .05f};
    EnhThrow data = {.ball=item, .reticle=sizes[s->config.quality], .bullseye=1, .spinning=!!(
            s->config.flags & SILPH_CFG_CURVE)};
    void *args[] = {&data, NULL}, *promise = NULL;
    capture_begin(s, a->expected, SILPH_SOURCE_WILD, now);
    s->metrics.throws++;
    if (!invoke(s, a->capture, map, args, &promise)) {
        capture_abandon(s);
        return SILPH_MANAGED_EXCEPTION;
    }
    if (!promise) {
        capture_abandon(s);
        return SILPH_MANAGED_NOT_READY;
    }
    if (!keep(s, promise, &s->promise, false)) {
        capture_abandon(s);
        return SILPH_MANAGED_CAPACITY;
    }
    s->promise_at = now;
    s->promise_id = a->expected;
    s->promise_shiny = s->shiny;
    if (!keep(s, state, &s->promise_state, true))return SILPH_MANAGED_CAPACITY;
    ar_resolve_gate(s, 25);
    a->ui_at = now + UINT64_C(400000000);
    return 0;
}

static int ar_capture(Enhancer *s, uint64_t now) {
    return s->route.headless ? ar_hidden_capture(s, now) : ar_visual_capture(s, now);
}

static void ar_refresh(Enhancer *s, const SilphSchedulePermit *permit, uint64_t now) {
    AutoRaid *a = &s->route;
    if (a->map_disabled || now < a->map_at)return;
    a->map_at = now + UINT64_C(8000000000);
    int r = 0;
    if (!a->map_handler.serial)r = silph_game_read_rocket_service(a->game, 5, &a->map_handler);
    if (!r && !a->map_settings.serial) {
        SilphManagedRef settings = {0};
        SilphReadValue global = {0}, value = {0};
        r = silph_read_field_type(a->read, a->map_handler, "Niantic.Holoholo", "IGlobalSettings",
                                  &settings);
        if (!r)
            r = silph_read_call(a->read, settings, "get_Settings", SILPH_READ_REFERENCE, NULL, 0,
                                &global);
        if (!r)
            r = silph_read_field(a->read, global.reference, "MapSettings", SILPH_READ_REFERENCE,
                                 &value);
        if (!r) {
            a->map_settings = value.reference;
            value.reference = (SilphManagedRef) {0};
        }
        SilphManagedRef refs[] = {settings, global.reference, value.reference};
        for (unsigned i = 0; i < 3; i++)
            if (refs[i].serial)
                silph_managed_release(a->managed, refs[i]);
    }
    if (!r && !a->map_tuned) {
        r = silph_rocket_map_settings(a->read, permit, a->map_settings, a->map_saved, false);
        if (!r)a->map_tuned = 1;
    }
    SilphReadValue result = {0};
    if (!r)
        r = silph_rocket_invoke(a->read, permit, RK_MAP_REFRESH, a->map_handler, NULL, 0, &result);
    if (result.reference.serial)silph_managed_release(a->managed, result.reference);
    if (r) {
        a->map_disabled = 1;
        __android_log_print(ANDROID_LOG_INFO, "SilphRaid", "map acceleration unavailable error=%d",
                            r);
    }
    else __android_log_print(ANDROID_LOG_INFO, "SilphRaid", "map refresh requested");
}

static int autoraid_tick(Enhancer *s, const SilphSchedulePermit *permit, uint64_t now) {
    AutoRaid *a = &s->route;
    if (!a->link)return 0;
    int r = ar_commands(s, now);
    if (r)return r;
    ar_graphics(s, now, false);
    if (!silph_raid_map_accelerate(a->phase, a->surveying))ar_map_idle(s);
    if (a->phase == 1) {
        if (a->surveying && !a->survey_row)ar_refresh(s, permit, now);
        ar_survey_tick(s, now);
    }
    if (a->phase == 1 && now >= a->next) {
        a->next = now + UINT64_C(5000000000);
        SilphRaidSite current = {0};
        if (!silph_raid_read_current(a->reader, &current) && current.verified &&
            strcmp(current.gym, a->wait_gym)) {
            memcpy(a->wait_gym, current.gym, sizeof(a->wait_gym));
            SilphReadValue location = {0}, lat = {0}, lon = {0};
            double latitude = 0, longitude = 0;
            if (!silph_read_call(a->read, a->root, "get_FortLocation", SILPH_READ_BOXED, NULL, 0,
                                 &location) && location.reference.serial) {
                if (!silph_read_field(a->read, location.reference, "Latitude", SILPH_READ_F64,
                                      &lat))
                    memcpy(&latitude, &lat.bits, 8);
                if (!silph_read_field(a->read, location.reference, "Longitude", SILPH_READ_F64,
                                      &lon))
                    memcpy(&longitude, &lon.bits, 8);
                silph_managed_release(a->managed, location.reference);
            }
            __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                                "waiting raid observed gym=%s species=%u level=%u form=%u costume=%u evolution=%u lat=%.8f lon=%.8f end=%llu server=%llu",
                                current.gym, current.species, current.level, current.form,
                                current.costume, current.evolution, latitude, longitude,
                                (unsigned long long) current.end_ms,
                                (unsigned long long) current.server_ms);
        }
    }
    if (a->phase == 3) {
        ar_refresh(s, permit, now);
        r = ar_scan(s, now);
        if (r)return r;
        if (a->phase == 3 && !a->nudged && now >= a->nudge_at) {
            a->nudged = 1;
            ar_phase(s, 6, 0, 0, now + UINT64_C(15000000000));
        }
    }
    if ((a->phase == 2 || a->phase == 6) && now >= a->deadline)ar_done(s, 2);
    if (a->phase == 4) {
        if (now >= a->deadline) {
            ar_exit(s);
            ar_phase(s, 8, 0, 7, 0);
        }
        else if (now >= a->next) {
            a->next = now + UINT64_C(1000000000);
            double latitude = 0, longitude = 0;
            if (!silph_game_read_rocket_position(a->game, &latitude, &longitude) &&
                (fabs(latitude - a->capture_latitude) > 0.000001 ||
                 fabs(longitude - a->capture_longitude) > 0.000001)) {
                a->capture_latitude = latitude;
                a->capture_longitude = longitude;
                a->dirty = 1;
            }
        }
    }
    if (a->phase == 5 && a->stage != 8 && now >= a->deadline) {
        __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                            "stage timeout stage=%u seed=%llu paid=%u gui=%s", a->stage,
                            (unsigned long long) a->site.seed, a->ticket_started, a->last_gui);
        if (a->stage <= 4 || a->stage == 9)ar_done(s, 7);
        else {
            ar_exit(s);
            ar_phase(s, 8, 0, a->stage == 6 ? 11 : 7, 0);
        }
    }
    if (a->phase == 5 && a->stage == 8) {
        unsigned recovery = silph_raid_restore_decision(ar_map_restored(s), now >= a->deadline,
                                                        a->restore_attempts, now, a->restore_at);
        if (recovery == 1) {
            if (s->raid.serial)raid_clear(s);
            drop(s, &a->place);
            drop(s, &a->reward);
            ar_phase(s, 7, 0, a->leave_reason, 0);
        }
        else if (recovery >= 2) {
            if (recovery == 3) {
                __android_log_print(ANDROID_LOG_WARN, "SilphRaid",
                                    "map recovery exhausted; clean session recovery requested");
                ar_phase(s, 8, 0, 21, 0);
            }
            else {
                a->restore_attempts++;
                a->restore_at = now + UINT64_C(5000000000);
                ar_exit(s);
                __android_log_print(ANDROID_LOG_INFO, "SilphRaid", "map restore retry=%u maximum=2",
                                    a->restore_attempts);
            }
        }
        goto route_metrics;
    }
    if (a->phase == 5) {
        if (a->resume && a->stage < 6 && s->interaction.serial) {
            void *state = NULL, *map = NULL;
            if (target(s, s->state, &state) && invoke(s, s->state_map, state, NULL, &map) &&
                exact(s, map, a->map_pokemon_class)) {
                a->won = 1;
                ar_phase(s, 5, 6, 0, now + UINT64_C(180000000000));
                a->ui_at = now;
            }
        }
        if (a->stage == 9 && a->rejoining && s->raid.serial && s->raid.serial != a->battle_ref &&
            s->raid_bound && s->raid_status == 1) {
            a->rejoining = 0;
            ar_phase(s, 5, 4, 0, a->deadline);
            __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                                "same battle resumed after team recovery");
        }
        if (s->raid.serial && s->raid_bound && s->raid_status == 1 && a->stage != 9) {
            if (a->stage != 4)ar_phase(s, 5, 4, 0, a->deadline);
        }
        else if (s->raid.serial && s->raid_bound && s->raid_status > 2 && s->raid_status != 3 &&
                 a->stage != 9) {
            if (s->raid_status == 5 && a->stage == 4 && ar_owns_gui(s) &&
                ar_team_exhausted(s))
                ar_recover_team(s);
            else {
                ar_done(s, 9);
                return 0;
            }
        }
        if (a->won && a->stage == 6 && a->expected && s->metrics.encounter == a->expected &&
            (s->metrics.catches > a->caught_before || s->metrics.fled > a->fled_before)) {
            unsigned reason = s->metrics.catches > a->caught_before ? 0 : !a->balls ? 2 : 3;
            if (a->headless) {
                r = ar_capture_finished(s, reason);
                if (r)return r;
            }
            else ar_phase(s, 5, 7, reason, a->deadline);
            a->caught_before = s->metrics.catches;
            a->fled_before = s->metrics.fled;
        }
        if (a->stage == 7 && s->map && !s->care_pending.serial && !s->care_ready &&
            !s->transfer_pending.serial) { ar_phase(s, 7, 0, a->reason, 0); }
        if (a->stage == 6) {
            r = ar_capture(s, now);
            if (r)return r;
        }
        if (now >= a->next) {
            a->next = now + UINT64_C(200000000);
            void *service = NULL, *gui = NULL;
            if (target(s, a->gui, &service) && read_ref(s, service, "TopGui", &gui)) {
                const char *name = s->api.class_name(s->base.object_get_class(gui));
                if (name && strcmp(a->last_gui, name)) {
                    snprintf(a->last_gui, sizeof(a->last_gui), "%s", name);
                    __android_log_print(ANDROID_LOG_INFO, "SilphRaid", "gui=%s", name);
                }
                if (name && !strcmp(name, "RaidApproachUIController") && a->stage <= 2 &&
                    ar_owns_gui(s))
                    r = ar_approach(s, permit, gui, now);
                else if (name && !strcmp(name, "MainMenuGuiController") && s->map &&
                         a->stage == 1 && !a->joined && now - a->ui_at > UINT64_C(8000000000)) {
                    if (a->reopen++ >= 2) {
                        ar_done(s, 2);
                        return 0;
                    }
                    void *place = NULL, *explore = NULL;
                    if (target(s, a->place, &place) && target(s, a->explore, &explore)) {
                        bool remote = false;
                        int32_t context = 0;
                        const char *types[] = {"Niantic.Holoholo.Gym.MapGym", "System.Boolean",
                                               "System.Action",
                                               "Niantic.Holoholo.Gym.PreviousContext"};
                        void *args[] = {place, &remote, NULL, &context};
                        if (!invoke(s, method(s, s->base.object_get_class(explore), "GymSelected",
                                              "System.Void", 4, types), explore, args, NULL))
                            return SILPH_MANAGED_EXCEPTION;
                        a->ui_at = now;
                    }
                } else if (name && !strcmp(name, "UseRaidTicketGuiController") && a->joined &&
                           a->stage == 2 && !a->ticket_started && ar_owns_gui(s)) {
                    if (a->resume) {
                        ar_phase(s, 8, 0, 10, 0);
                        return 0;
                    }
                    uint32_t wanted = a->desired_pass;
                    if (wanted && ar_cost(s, gui, wanted)) {
                        if (!ar_void(s, gui, "StartAnimation")) {
                            ar_done(s, 7);
                            return 0;
                        }
                        else {
                            a->ticket_started = 1;
                            __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                                                "entry selected item=%u", wanted);
                        }
                    }
                    else if (wanted && now > a->ui_at + UINT64_C(500000000)) {
                        ar_void(s, gui, "CycleCurrency");
                        a->ui_at = now;
                    }
                } else if (name && !strcmp(name, "RaidLobbyGuiController") &&
                           (a->stage == 1 || a->stage == 2 || a->stage == 3 || a->stage == 4 ||
                            a->stage == 9)) {
                    if (!a->joined && a->stage == 1 && ar_owns_gui(s)) {
                        SilphRaidSite current = {0};
                        bool started = true;
                        if (!silph_raid_read_current(a->reader, &current) && current.verified &&
                            current.full_details && current.pass_known && current.ticket_used &&
                            !current.complete && current.seed == a->site.seed &&
                            !strcmp(current.gym, a->site.gym) && scalar(s, method(s,
                                                                                  s->base.object_get_class(
                                                                                          gui),
                                                                                  "get_BattleStarted",
                                                                                  "System.Boolean",
                                                                                  0, NULL), gui,
                                                                        &started, 1)) {
                            if (started || a->resume) {
                                ar_done(s, 9);
                                return 0;
                            }
                            a->joined = a->ticket_started = 1;
                            a->desired_pass = 0;
                            ar_phase(s, 5, 3, 0, a->deadline);
                            __android_log_print(ANDROID_LOG_INFO, "SilphRaid",
                                                "paid lobby resumed seed=%llu; no ticket requested",
                                                (unsigned long long) a->site.seed);
                        }
                    }
                    if (a->joined) {
                        if (a->resume) {
                            ar_void(s, gui, "LeaveLobby");
                            ar_phase(s, 8, 0, 10, 0);
                        }
                        else r = ar_lobby(s, permit, gui, now);
                    }
                } else if (name && !strcmp(name, "RaidResolveUIController") &&
                           (a->stage == 4 || a->stage == 9 ||
                            (a->joined && (a->stage == 2 || a->stage == 3)) ||
                            (a->resume && a->stage <= 2)))
                    r = ar_resolve(s, gui, now);
                else if (name &&
                         (!strcmp(name, "ModalDialog") || !strcmp(name, "WarningModalDialog") ||
                          !strcmp(name, "LargePlayerWarningModal")))
                    ar_modal(s, gui, now);
            }
        }
    }
    route_metrics:
    s->metrics.raid_request = a->request;
    s->metrics.raid_phase = a->phase;
    s->metrics.raid_stage = a->stage;
    s->metrics.raid_reason = a->reason;
    return r;
}

static int autoraid_close(Enhancer *s) {
    AutoRaid *a = &s->route;
    ar_graphics(s, clock_ns(), true);
    int r = a->pass ? silph_raid_pass_close(a->pass) : 0;
    if (r)return r;
    free(a->pass);
    a->pass = NULL;
    if (a->reader) {
        r = silph_raid_read_close(a->reader);
        if (r)return r;
        free(a->reader);
        a->reader = NULL;
    }
    if (a->map_settings.serial) {
        if (a->map_tuned) {
            r = silph_rocket_map_settings(a->read, NULL, a->map_settings, a->map_saved, true);
            if (r)return r;
            a->map_tuned = 0;
        }
        r = silph_managed_release(a->managed, a->map_settings);
        if (r)return r;
        a->map_settings = (SilphManagedRef) {0};
    }
    if (a->map_handler.serial) {
        r = silph_managed_release(a->managed, a->map_handler);
        if (r)return r;
        a->map_handler = (SilphManagedRef) {0};
    }
    if (a->model.serial && !ar_drop_capture_model(s))return SILPH_MANAGED_EXCEPTION;
    free(a->pass);
    free(a->reader);
    a->pass = a->reader = NULL;
    drop(s, &a->recovery_pending);
    drop(s, &a->medicine_list);
    drop(s, &a->explore);
    drop(s, &a->root);
    drop(s, &a->gui);
    drop(s, &a->place);
    drop(s, &a->reward);
    drop(s, &a->factory);
    return 0;
}
