#define _GNU_SOURCE

#include "raid-pass.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    void *read, *managed, *promise;
    SilphManagedRef service;
    SilphScheduleIdentity identity;
    SilphRaidPassMetrics metrics;
    uint64_t seed, sent_at;
    uint32_t attempts;
    char gym[161];
    bool opened, attempted;
} RaidPass;

size_t silph_raid_pass_size(void) { return sizeof(RaidPass); }

int
silph_raid_pass_open(void *memory, size_t size, void *read, void *managed, SilphManagedRef service,
                     const SilphScheduleIdentity *identity) {
    if (!memory || size != sizeof(RaidPass) || !read || !managed || !service.serial ||
        !identity)
        return SILPH_MANAGED_ARGUMENT;
    RaidPass *s = memory;
    if (s->opened)return SILPH_MANAGED_STATE;
    void *object = NULL;
    int status = silph_managed_target(managed, service, &object);
    if (status)return status;
    *s = (RaidPass) {.read=read, .managed=managed, .identity=*identity};
    status = silph_managed_retain(managed, object, false, &s->service);
    if (!status) {
        s->promise = calloc(1, silph_managed_promise_size());
        if (!s->promise)status = SILPH_MANAGED_CAPACITY;
    }
    s->opened = true;
    if (status)silph_raid_pass_close(s);
    return status;
}

int silph_raid_pass_request(void *memory, const SilphSchedulePermit *permit, const char *gym,
                            uint64_t seed, uint64_t now) {
    RaidPass *s = memory;
    if (!s || !s->opened || !gym || !seed || !now)return SILPH_MANAGED_ARGUMENT;
    size_t length = strnlen(gym, 161);
    if (!length || length > 160)return SILPH_MANAGED_ARGUMENT;
    for (size_t i = 0; i < length; i++) {
        unsigned c = (unsigned char) gym[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '.' || c == '_' || c == '-'))
            return SILPH_MANAGED_ARGUMENT;
    }
    bool same = s->attempted && s->seed == seed && !strcmp(s->gym, gym);
    if (s->metrics.pending || s->metrics.uncertain || !silph_schedule_permitted(permit, true))
        return SILPH_MANAGED_STATE;
    if (same && (s->metrics.result != 4 || s->attempts >= 3))return SILPH_MANAGED_STATE;
    if (same && (now < s->sent_at || now - s->sent_at < UINT64_C(3000000000)))
        return SILPH_MANAGED_NOT_READY;
    SilphManagedRef id = {0};
    SilphReadValue value = {0};
    int status = silph_managed_ascii(s->managed, gym, (uint32_t) length, &id);
    if (!status) {
        bool submitted = false;
        s->metrics.error = 0;
        status = silph_raid_award_free(s->read, permit, s->service, id, seed, &value, &submitted);
        if (submitted) {
            s->metrics.requests++;
            s->metrics.result = 0;
            s->metrics.raid_seed = seed;
            memcpy(s->metrics.gym, gym, length + 1);
            s->attempts = same ? s->attempts + 1 : 1;
            s->sent_at = now;
            s->attempted = true;
            s->seed = seed;
            memcpy(s->gym, gym, length + 1);
            if (status)s->metrics.uncertain = 1;
        }
        if (!status) {
            s->metrics.pending = 1;
            status = silph_managed_promise_open(s->promise, silph_managed_promise_size(), s->read,
                                                s->managed, value.reference, &s->identity,
                                                s->metrics.requests, seed, now, "Holoholo.Rpc",
                                                "AwardFreeRaidTicketOutProto");
            if (status)s->metrics.uncertain = 1;
        }
    }
    SilphManagedRef *refs[] = {&value.reference, &id};
    for (unsigned i = 0; i < 2; i++)
        if (refs[i]->serial) {
            int freed = silph_managed_release(s->managed, *refs[i]);
            if (!status)status = freed;
        }
    if (status)s->metrics.error = (uint32_t) status;
    return status;
}

int silph_raid_pass_poll(void *memory, uint64_t now, SilphRaidPassMetrics *out) {
    RaidPass *s = memory;
    if (!s || !s->opened || !out || !now)return SILPH_MANAGED_ARGUMENT;
    int status = 0;
    if (s->metrics.pending && !s->metrics.uncertain) {
        SilphPromiseMetrics promise = {0};
        status = silph_managed_promise_poll(s->promise, &s->identity, now, &promise);
        if (!status && promise.state == SILPH_PROMISE_WAITING) {
            *out = s->metrics;
            return 0;
        }
        if (!status && promise.state != SILPH_PROMISE_RESOLVED)status = SILPH_MANAGED_NOT_READY;
        SilphManagedRef response = {0};
        bool fresh = false;
        SilphReadValue result = {0};
        if (!status)
            status = silph_managed_promise_take(s->promise, &s->identity, now, &response, &fresh);
        if (!status && !fresh)status = SILPH_MANAGED_STALE;
        if (!status)
            status = silph_read_field(s->read, response, "Result", SILPH_READ_ENUM32, &result);
        if (!status && (result.bits < 1 || result.bits > 5))status = SILPH_MANAGED_CONTRACT;
        if (!status)s->metrics.result = (uint32_t) result.bits;
        if (response.serial) {
            int freed = silph_managed_release(s->managed, response);
            if (!status)status = freed;
        }
        int closed = silph_managed_promise_close(s->promise);
        if (!status)status = closed;
        s->metrics.pending = 0;
        if (status) {
            s->metrics.error = (uint32_t) status;
            s->metrics.uncertain = 1;
        }
    }
    *out = s->metrics;
    return status;
}

int silph_raid_pass_close(void *memory) {
    RaidPass *s = memory;
    if (!s || !s->opened)return 0;
    int status = s->promise ? silph_managed_promise_close(s->promise) : 0;
    if (status)return status;
    if (s->service.serial) {
        status = silph_managed_release(s->managed, s->service);
        if (status)return status;
        s->service = (SilphManagedRef) {0};
    }
    free(s->promise);
    memset(s, 0, sizeof(*s));
    return 0;
}
