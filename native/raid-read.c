#include "raid-read.h"
#include <string.h>
#include <math.h>

typedef struct {
    void *read, *managed;
    SilphManagedRef directory, iterator, container, rpc;
    unsigned opened;
} RaidRead;

size_t silph_raid_read_size(void) { return sizeof(RaidRead); }

static int drop(RaidRead *s, SilphManagedRef *ref) {
    int code = ref->serial ? silph_managed_release(s->managed, *ref) : 0;
    if (!code)*ref = (SilphManagedRef) {0};
    return code;
}

static int invoke(RaidRead *s, SilphManagedRef object, const char *name, uint32_t kind,
                  SilphReadValue *out) {
    return silph_read_call(s->read, object, name, kind, NULL, 0, out);
}

static int end_scan(RaidRead *s) {
    SilphReadValue value = {0};
    int status = s->iterator.serial ? invoke(s, s->iterator, "Dispose", SILPH_READ_VOID, &value)
                                    : 0;
    int freed = drop(s, &s->iterator);
    return status ? status : freed;
}

int silph_raid_read_end(void *memory) {
    RaidRead *s = memory;
    return !s || !s->opened ? SILPH_MANAGED_ARGUMENT : end_scan(s);
}

int silph_raid_read_close(void *memory) {
    RaidRead *s = memory;
    if (!s || !s->opened)return SILPH_MANAGED_ARGUMENT;
    int status = end_scan(s);
    SilphManagedRef *refs[] = {&s->directory, &s->container, &s->rpc};
    for (unsigned i = 0; i < 3; i++) {
        int freed = drop(s, refs[i]);
        if (!status)status = freed;
    }
    if (!status)memset(s, 0, sizeof(*s));
    return status;
}

static int resolve(RaidRead *s, const char *space, const char *name, SilphManagedRef *out) {
    SilphManagedRef type = {0};
    SilphReadValue value = {0};
    int status = silph_read_type_object(s->read, "holo-game.dll", space, name, &type);
    SilphReadArgument arg = {.kind=SILPH_READ_REFERENCE, .value.reference=type};
    if (!status)
        status = silph_read_call(s->read, s->container, "TryResolve", SILPH_READ_REFERENCE, &arg, 1,
                                 &value);
    if (!status) {
        *out = value.reference;
        value.reference = (SilphManagedRef) {0};
    }
    int a = drop(s, &type), b = drop(s, &value.reference);
    return status ? status : a ? a : b;
}

int silph_raid_read_open(void *memory, size_t bytes, void *read, void *managed, void *map) {
    if (!memory || bytes != sizeof(RaidRead) || !read || !managed || !map)
        return SILPH_MANAGED_ARGUMENT;
    RaidRead *s = memory;
    if (s->opened)return SILPH_MANAGED_STATE;
    *s = (RaidRead) {.read=read, .managed=managed, .opened=1};
    SilphManagedRef held = {0}, container = {0}, type = {0};
    int status = silph_managed_retain(managed, map, false, &held);
    if (!status)status = silph_read_field_type(read, held, "Zenject", "DiContainer", &container);
    if (!status)
        status = silph_read_type_object(read, "holo-game.dll", "Niantic.Holoholo.Map",
                                        "IMapPlaceDirectoryService", &type);
    if (!status) {
        SilphReadArgument arg = {.kind=SILPH_READ_REFERENCE, .value.reference=type};
        SilphReadValue value = {0};
        status = silph_read_call(read, container, "TryResolve", SILPH_READ_REFERENCE, &arg, 1,
                                 &value);
        s->directory = value.reference;
        if (!status && !s->directory.serial)status = SILPH_MANAGED_NOT_READY;
    }
    if (!status) {
        s->container = container;
        container = (SilphManagedRef) {0};
        status = resolve(s, "Niantic.Holoholo.Raid", "IRaidRpcService", &s->rpc);
    }
    SilphManagedRef *refs[] = {&type, &container, &held};
    for (unsigned i = 0; i < 3; i++) {
        int freed = drop(s, refs[i]);
        if (!status)status = freed;
    }
    if (status)silph_raid_read_close(s);
    return status;
}

int silph_raid_read_begin(void *memory) {
    RaidRead *s = memory;
    if (!s || !s->opened)return SILPH_MANAGED_ARGUMENT;
    int status = end_scan(s);
    SilphReadValue dictionary = {0}, values = {0}, iterator = {0};
    if (!status)
        status = silph_read_field(s->read, s->directory, "emlf", SILPH_READ_REFERENCE, &dictionary);
    if (!status)
        status = invoke(s, dictionary.reference, "get_Values", SILPH_READ_REFERENCE, &values);
    if (!status)status = invoke(s, values.reference, "GetEnumerator", SILPH_READ_BOXED, &iterator);
    if (!status)s->iterator = iterator.reference; else drop(s, &iterator.reference);
    int a = drop(s, &values.reference), b = drop(s, &dictionary.reference);
    return status ? status : a ? a : b;
}

static int
scalar_field(RaidRead *s, SilphManagedRef object, const char *field, uint32_t kind, uint64_t *out) {
    SilphReadValue value = {0};
    int status = silph_read_field(s->read, object, field, kind, &value);
    if (!status)*out = value.bits;
    return status;
}

static int read_info(RaidRead *s, SilphManagedRef raid, bool simple, SilphRaidSite *out) {
    SilphReadValue pokemon = {0}, display = {0}, value = {0};
    uint64_t n = 0;
    int status = scalar_field(s, raid, "RaidSeed", SILPH_READ_I64, &out->seed);
    if (!status)status = scalar_field(s, raid, "RaidBattleMs", SILPH_READ_I64, &out->battle_ms);
    if (!status)status = scalar_field(s, raid, "RaidEndMs", SILPH_READ_I64, &out->end_ms);
    if (!status) {
        status = simple ? invoke(s, raid, "get_RaidLevel", SILPH_READ_ENUM32, &value)
                        : silph_read_field(s->read, raid, "RaidLevel", SILPH_READ_ENUM32, &value);
        out->level = (uint32_t) value.bits;
    }
    if (!status && !simple) {
        status = scalar_field(s, raid, "Complete", SILPH_READ_BOOL, &n);
        out->complete = !!n;
    }
    if (!status && simple) {
        status = scalar_field(s, raid, "FreeTicketAvailable", SILPH_READ_BOOL, &n);
        out->free_available = !!n;
    }
    if (!status)
        status = silph_read_field(s->read, raid, "RaidPokemon", SILPH_READ_REFERENCE, &pokemon);
    if (!status && pokemon.reference.serial) {
        status = invoke(s, pokemon.reference, "get_PokemonId", SILPH_READ_I32, &value);
        out->species = (uint32_t) value.bits;
        if (!status)
            status = invoke(s, pokemon.reference, "get_PokemonDisplay", SILPH_READ_REFERENCE,
                            &display);
        if (!status && display.reference.serial) {
            status = scalar_field(s, display.reference, "Form", SILPH_READ_ENUM32, &n);
            out->form = (uint32_t) n;
            if (!status) {
                status = scalar_field(s, display.reference, "Costume", SILPH_READ_ENUM32, &n);
                out->costume = (uint32_t) n;
            }
            if (!status) {
                status = scalar_field(s, display.reference, "Gender", SILPH_READ_ENUM32, &n);
                out->gender = (uint32_t) n;
            }
            out->display_known = !status;
            if (!status) {
                SilphReadValue evolution = {0};
                if (!invoke(s, display.reference, "get_TempEvoType", SILPH_READ_ENUM32,
                            &evolution) && evolution.bits <= 100) {
                    out->evolution = (uint32_t) evolution.bits;
                    out->evolution_known = 1;
                }
            }
        }
    }
    out->verified = !status && out->seed && out->end_ms > out->battle_ms;
    out->active = out->verified && out->species && !out->complete;
    int a = drop(s, &display.reference), b = drop(s, &pokemon.reference);
    return status ? status : a ? a : b;
}

static int cached_info(RaidRead *s, SilphManagedRef id, SilphRaidSite *out) {
    if (!s->rpc.serial)return 0;
    SilphReadValue dictionary = {0}, list = {0}, count = {0}, item = {0}, full = {0}, simple = {
            0}, raid = {0}, found = {0};
    int status = silph_read_field(s->read, s->rpc, "elhw", SILPH_READ_REFERENCE, &dictionary);
    SilphReadArgument key = {.kind=SILPH_READ_REFERENCE, .value.reference=id};
    if (!status)
        status = silph_read_call(s->read, dictionary.reference, "ContainsKey", SILPH_READ_BOOL,
                                 &key, 1, &found);
    if (!status && found.bits)
        status = silph_read_call(s->read, dictionary.reference, "get_Item", SILPH_READ_REFERENCE,
                                 &key, 1, &list);
    if (!status && list.reference.serial)
        status = invoke(s, list.reference, "get_Count", SILPH_READ_I32, &count);
    if (!status && count.bits > 64)status = SILPH_MANAGED_CONTRACT;
    for (uint64_t i = 0; !status && i < count.bits; i++) {
        SilphReadArgument index = {.kind=SILPH_READ_I32, .value.bits=i};
        status = silph_read_call(s->read, list.reference, "get_Item", SILPH_READ_REFERENCE, &index,
                                 1, &item);
        if (!status)
            status = silph_read_field(s->read, item.reference, "details", SILPH_READ_REFERENCE,
                                      &full);
        if (!status)
            status = silph_read_field(s->read, item.reference, "simpleDetails",
                                      SILPH_READ_REFERENCE, &simple);
        bool is_simple = !full.reference.serial;
        if (!status && full.reference.serial)
            status = silph_read_field(s->read, full.reference, "RaidInfo", SILPH_READ_REFERENCE,
                                      &raid);
        else if (!status && simple.reference.serial)
            status = silph_read_field(s->read, simple.reference, "ApproachScreenDetails",
                                      SILPH_READ_REFERENCE, &raid);
        if (!status && raid.reference.serial) {
            SilphRaidSite site = {.latitude=out->latitude, .longitude=out->longitude, .state=out->state};
            memcpy(site.gym, out->gym, sizeof(site.gym));
            status = read_info(s, raid.reference, is_simple, &site);
            SilphManagedRef details = is_simple ? simple.reference : full.reference;
            uint64_t result = 0;
            if (!status)status = scalar_field(s, details, "Result", SILPH_READ_ENUM32, &result);
            site.details_result = (uint32_t) result;
            if (!status)
                status = scalar_field(s, details, "ServerMs", SILPH_READ_I64, &site.server_ms);
            site.pass_known = !status && result == 1 && site.server_ms > 0;
            if (!status && full.reference.serial) {
                site.full_details = 1;
                uint64_t n = 0;
                status = scalar_field(s, full.reference, "FreeTicketAvailable", SILPH_READ_BOOL,
                                      &n);
                site.free_available = !!n;
                if (!status) {
                    status = scalar_field(s, full.reference, "TicketUsed", SILPH_READ_BOOL, &n);
                    site.ticket_used = !!n;
                }
                if (!status) {
                    status = scalar_field(s, full.reference, "RemoteTicketUsed", SILPH_READ_BOOL,
                                          &n);
                    site.remote_used = !!n;
                }
                if (!status) {
                    status = scalar_field(s, full.reference, "NumPlayersInLobby", SILPH_READ_I32,
                                          &n);
                    site.players = (uint32_t) n;
                }
                if (!status) {
                    status = scalar_field(s, full.reference, "PlayerCanJoinLobby", SILPH_READ_BOOL,
                                          &n);
                    site.can_join = !!n;
                }
                if (!status) {
                    status = scalar_field(s, full.reference, "IsWithinPlfeRange", SILPH_READ_BOOL,
                                          &n);
                    site.in_range = !!n;
                }
                if (!status) {
                    status = scalar_field(s, full.reference, "ThrowsRemaining", SILPH_READ_I32, &n);
                    site.throws_remaining = (int32_t) n;
                }
                if (!status) {
                    status = scalar_field(s, full.reference, "ReceivedRewards", SILPH_READ_BOOL,
                                          &n);
                    site.rewards_claimed = !!n;
                }
                if (!status) {
                    status = scalar_field(s, full.reference, "RvnBattleFlushed", SILPH_READ_BOOL,
                                          &n);
                    site.flushed = !!n;
                }
            }
            if (!status && site.verified && (!out->seed || site.seed == out->seed) &&
                (!out->pass_known || site.server_ms >= out->server_ms))
                *out = site;
        }
        SilphManagedRef *refs[] = {&raid.reference, &simple.reference, &full.reference,
                                   &item.reference};
        for (unsigned j = 0; j < 4; j++) {
            int freed = drop(s, refs[j]);
            if (!status)status = freed;
        }
    }
    SilphManagedRef *refs[] = {&raid.reference, &simple.reference, &full.reference, &item.reference,
                               &list.reference, &dictionary.reference};
    for (unsigned i = 0; i < 6; i++) {
        int freed = drop(s, refs[i]);
        if (!status)status = freed;
    }
    return status;
}

int silph_raid_read_next(void *memory, SilphRaidSite *out, bool *finished) {
    RaidRead *s = memory;
    if (!s || !s->opened || !s->iterator.serial || !out || !finished)return SILPH_MANAGED_ARGUMENT;
    *out = (SilphRaidSite) {0};
    *finished = false;
    SilphReadValue value = {0}, current = {0}, id = {0}, location = {0}, fort = {0}, raid = {
            0}, pokemon = {0}, display = {0};
    int status = invoke(s, s->iterator, "MoveNext", SILPH_READ_BOOL, &value);
    if (!status && !value.bits) {
        *finished = true;
        return end_scan(s);
    }
    if (!status)status = invoke(s, s->iterator, "get_Current", SILPH_READ_REFERENCE, &current);
    if (!status)status = invoke(s, current.reference, "get_Id", SILPH_READ_REFERENCE, &id);
    uint32_t length = 0;
    if (!status)
        status = silph_read_utf8(s->read, id.reference, out->gym, sizeof(out->gym), &length);
    if (!status && (!length || length > 160))status = SILPH_MANAGED_CONTRACT;
    if (!status)status = invoke(s, current.reference, "get_Location", SILPH_READ_BOXED, &location);
    if (!status) {
        status = silph_read_field(s->read, location.reference, "Latitude", SILPH_READ_F64, &value);
        if (!status)memcpy(&out->latitude, &value.bits, 8);
    }
    if (!status) {
        status = silph_read_field(s->read, location.reference, "Longitude", SILPH_READ_F64, &value);
        if (!status)memcpy(&out->longitude, &value.bits, 8);
    }
    if (!status &&
        (!isfinite(out->latitude) || !isfinite(out->longitude) || fabs(out->latitude) > 90 ||
         fabs(out->longitude) > 180))
        status = SILPH_MANAGED_CONTRACT;
    if (!status) {
        status = invoke(s, current.reference, "get_CurrentRaidState", SILPH_READ_ENUM32, &value);
        out->state = (uint32_t) value.bits;
    }
    if (!status)status = invoke(s, current.reference, "get_FortData", SILPH_READ_REFERENCE, &fort);
    if (!status)
        status = silph_read_field(s->read, fort.reference, "RaidInfo", SILPH_READ_REFERENCE, &raid);
    if (!status && raid.reference.serial)status = read_info(s, raid.reference, false, out);
    if (!status)status = cached_info(s, id.reference, out);
    SilphManagedRef *refs[] = {&display.reference, &pokemon.reference, &raid.reference,
                               &fort.reference, &location.reference, &id.reference,
                               &current.reference};
    for (unsigned i = 0; i < 7; i++) {
        int freed = drop(s, refs[i]);
        if (!status)status = freed;
    }
    return status;
}

int silph_raid_read_current(void *memory, SilphRaidSite *out) {
    RaidRead *s = memory;
    if (!s || !s->opened || !out)return SILPH_MANAGED_ARGUMENT;
    *out = (SilphRaidSite) {0};
    SilphManagedRef root = {0};
    SilphReadValue id = {0}, raid = {0}, value = {0};
    int status = resolve(s, "Niantic.Holoholo.Gym", "IGymRootController", &root);
    if (!status && root.serial) {
        status = invoke(s, root, "get_IsActive", SILPH_READ_BOOL, &value);
        if (!status && value.bits) {
            status = invoke(s, root, "get_FortId", SILPH_READ_REFERENCE, &id);
            uint32_t length = 0;
            if (!status)
                status = silph_read_utf8(s->read, id.reference, out->gym, sizeof(out->gym),
                                         &length);
            if (!status)status = invoke(s, root, "get_RaidInfo", SILPH_READ_REFERENCE, &raid);
            if (!status && raid.reference.serial)status = read_info(s, raid.reference, false, out);
            if (!status)status = cached_info(s, id.reference, out);
            if (!status) {
                status = invoke(s, root, "get_IsPlayerInRange", SILPH_READ_BOOL, &value);
                out->in_range = !!value.bits;
            }
        }
    }
    SilphManagedRef *refs[] = {&raid.reference, &id.reference, &root};
    for (unsigned i = 0; i < 3; i++) {
        int freed = drop(s, refs[i]);
        if (!status)status = freed;
    }
    return status;
}

int silph_raid_read_gym(void *memory, const char *gym, SilphManagedRef *out) {
    RaidRead *s = memory;
    if (!s || !s->opened || !gym || !out)return SILPH_MANAGED_ARGUMENT;
    *out = (SilphManagedRef) {0};
    size_t n = strnlen(gym, 161);
    if (!n || n > 160)return SILPH_MANAGED_ARGUMENT;
    SilphManagedRef id = {0};
    SilphReadValue value = {0};
    int r = silph_managed_ascii(s->managed, gym, (uint32_t) n, &id);
    SilphReadArgument arg = {.kind=SILPH_READ_REFERENCE, .value.reference=id};
    if (!r)
        r = silph_read_call(s->read, s->directory, "GetGym", SILPH_READ_REFERENCE, &arg, 1, &value);
    if (!r) {
        *out = value.reference;
        value.reference = (SilphManagedRef) {0};
    }
    int a = drop(s, &id), b = drop(s, &value.reference);
    return r ? r : a ? a : b;
}
