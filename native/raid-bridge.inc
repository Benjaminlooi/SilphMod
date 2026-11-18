#include "raid-pass.h"

static int raid_cached_field(Read *s, SilphManagedRef ref, const char *name, uint32_t kind,
                             SilphReadValue *out) {
    clear_value(out);
    void *object = NULL;
    int status = silph_managed_target(s->managed, ref, &object);
    const void *field = status ? NULL : find_field(s, s->base.object_get_class(object), name, NULL,
                                                   NULL, &status);
    const void *type = field ? s->api.field_get_type(field) : NULL;
    if (!status && !matches_kind(s, type, kind))status = SILPH_MANAGED_CONTRACT;
    if (!status && kind == SILPH_READ_REFERENCE) {
        void *value = NULL;
        s->api.field_get_value(object, field, &value);
        status = accept_reference(s, type, value, out);
    }
    else if (!status) {
        s->api.field_get_value(object, field, &out->bits);
        if (kind == SILPH_READ_BOOL && out->bits > 1)status = SILPH_MANAGED_CONTRACT;
    }
    return status;
}

int silph_raid_award_free(void *memory, const SilphSchedulePermit *permit, SilphManagedRef service,
                          SilphManagedRef gym, uint64_t seed, SilphReadValue *out,
                          bool *submitted) {
    if (!out || !submitted)return SILPH_MANAGED_ARGUMENT;
    clear_value(out);
    *submitted = false;
    if (!seed || !silph_schedule_permitted(permit, true))return SILPH_MANAGED_STATE;
    Read *s = memory;
    int status = enter_read(s);
    if (status)return status;
    void *object = NULL, *id = NULL;
    status = silph_managed_target(s->managed, service, &object);
    if (!status)status = silph_managed_target(s->managed, gym, &id);
    if (!status && (!research_type(s, object, "Niantic.Holoholo.Raid", "RaidRpcService") ||
                    !research_type(s, id, "System", "String") || s->base.string_length(id) < 1 ||
                    s->base.string_length(id) > 160))
        status = SILPH_MANAGED_CONTRACT;
    SilphReadArgument args[] = {{.kind=SILPH_READ_REFERENCE, .value.reference=gym},
                                {.kind=SILPH_READ_I64, .value.bits=seed}};
    SilphReadValue details = {0}, raid = {0}, valid = {0}, available = {0}, actual = {0};
    if (!status)
        status = call_internal(s, service, NULL, "GetCachedRaidDetails", SILPH_READ_REFERENCE, args,
                               2, &details, NULL);
    if (!status && !details.reference.serial)status = SILPH_MANAGED_NOT_READY;
    if (!status) {
        void *value = NULL;
        status = silph_managed_target(s->managed, details.reference, &value);
        if (!status && !research_type(s, value, "Holoholo.Rpc", "GetRaidDetailsOutProto"))
            status = SILPH_MANAGED_CONTRACT;
    }
    if (!status)
        status = raid_cached_field(s, details.reference, "Result", SILPH_READ_ENUM32, &valid);
    if (!status && valid.bits != 1)status = SILPH_MANAGED_NOT_READY;
    if (!status)
        status = raid_cached_field(s, details.reference, "RaidInfo", SILPH_READ_REFERENCE, &raid);
    if (!status && !raid.reference.serial)status = SILPH_MANAGED_NOT_READY;
    if (!status)status = raid_cached_field(s, raid.reference, "RaidSeed", SILPH_READ_I64, &actual);
    if (!status && actual.bits != seed)status = SILPH_MANAGED_STALE;
    if (!status)
        status = raid_cached_field(s, details.reference, "FreeTicketAvailable", SILPH_READ_BOOL,
                                   &available);
    if (!status && !available.bits)status = SILPH_MANAGED_NOT_READY;
    if (!status) {
        *submitted = true;
        status = call_internal(s, service, NULL, "AwardFreeRaidTicket", SILPH_READ_REFERENCE, args,
                               1, out, permit);
    }
    int a = release_rpc_ref(s, &raid.reference), b = release_rpc_ref(s, &details.reference);
    return leave_read(s, status ? status : a ? a : b);
}
