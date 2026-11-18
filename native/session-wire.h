#ifndef SILPH_SESSION_WIRE_H
#define SILPH_SESSION_WIRE_H
#include <stddef.h>
#include <stdint.h>
#define SILPH_CONFIG_BYTES 1024u
#define SILPH_SNAPSHOT_BYTES 256u
#define SILPH_CONFIG_MAGIC UINT64_C(0x3147464348504c53)
#define SILPH_SNAPSHOT_MAGIC UINT64_C(0x3154415348504c53)
enum {SILPH_MODE_FARMER=1,SILPH_MODE_KECLEON=2,SILPH_MODE_ROCKET=3,SILPH_MODE_ENHANCER=5};
#ifdef SILPH_ENABLE_ENHANCER
#define SILPH_MODE_MAX 5u
#else
#define SILPH_MODE_MAX 3u
#endif
static inline int silph_mode_valid(uint32_t mode){return mode>=1&&mode<=SILPH_MODE_MAX&&mode!=4;}
#define SILPH_ENH_FAST 262144u
#define SILPH_ENH_INTRO 524288u
#define SILPH_ENH_ROCKET 1048576u
#define SILPH_ENH_TELEPORT 2097152u
#define SILPH_ENH_THROW 4194304u
#define SILPH_ENH_AIM 8388608u
#define SILPH_ENH_RAID 16777216u
#define SILPH_ENH_INFO 33554432u
#define SILPH_ENH_TRANSFER 67108864u
#define SILPH_ENH_RAID_HEAL 134217728u
#define SILPH_ENH_AUTORAID 268435456u
#define SILPH_ENH_LIGHT_GRAPHICS 536870912u
#define SILPH_ENH_FLAGS (SILPH_ENH_FAST|SILPH_ENH_INTRO|SILPH_ENH_ROCKET|SILPH_ENH_TELEPORT|SILPH_ENH_THROW|SILPH_ENH_AIM|SILPH_ENH_RAID|SILPH_ENH_INFO|SILPH_ENH_TRANSFER|SILPH_ENH_RAID_HEAL|SILPH_ENH_AUTORAID|SILPH_ENH_LIGHT_GRAPHICS)
enum {SILPH_CFG_CATCH=1,SILPH_CFG_SPIN=2,SILPH_CFG_CLEAN=4,SILPH_CFG_TRANSFER=8,SILPH_CFG_BERRIES=16,SILPH_CFG_CURVE=32,SILPH_CFG_READONLY=64,SILPH_CFG_TRAINER_NAME=128,SILPH_CFG_IV_NAMES=256,SILPH_CFG_FIX_LOCATION=512,SILPH_CFG_SCENES=1024,SILPH_CFG_MANUAL=2048,SILPH_CFG_GIFTS=4096,SILPH_CFG_BUDDY=8192,SILPH_CFG_OPEN_GIFTS=16384,SILPH_CFG_SEND_GIFTS=32768,SILPH_CFG_RECOMMENDED_TEAM=65536,SILPH_CFG_EVENT_ACTIONS=131072};
enum {SILPH_WIRE_OK=0,SILPH_WIRE_ARGUMENT=1,SILPH_WIRE_FORMAT=2,SILPH_WIRE_CRC=3};
enum {
    SILPH_FARMER_ACTIVITY_IDLE=1,SILPH_FARMER_ACTIVITY_SEARCH,SILPH_FARMER_ACTIVITY_ENCOUNTER,
    SILPH_FARMER_ACTIVITY_BERRY,SILPH_FARMER_ACTIVITY_CATCH,SILPH_FARMER_ACTIVITY_SYNC,
    SILPH_FARMER_ACTIVITY_TRANSFER,SILPH_FARMER_ACTIVITY_SPIN,SILPH_FARMER_ACTIVITY_DISCARD,
    SILPH_FARMER_ACTIVITY_RESEARCH,SILPH_FARMER_ACTIVITY_PASS,SILPH_FARMER_ACTIVITY_BUDDY,
    SILPH_FARMER_ACTIVITY_GIFTS,SILPH_FARMER_ACTIVITY_REMOVE_BUDDY
};
typedef struct {
    uint64_t ticks,session_hi,session_lo;
    uint32_t pid,mode,flags,quality,delay_ms,keep,max_catches,max_spins,max_encounters,berry_count;
    uint32_t berries[5],limit_count;
    struct {uint32_t item,floor;} limits[64];
    uint8_t species[256];
    uint32_t filters,duration_ms;
#ifdef SILPH_PILOT_ROCKET
    uint32_t potion_count,potion_order[4],revive_count,revive_order[2];
#endif
#ifdef SILPH_PRESENT_SCOPED
    uint8_t keep_species[136];
    uint32_t buddy_flags,buddy_berry_count,buddy_berries[5];
#endif
} SilphSessionConfig;
typedef struct {
    uint64_t ticks,session_hi,session_lo,sequence,account_hi,account_lo;
    uint64_t caught,fled,spun,discarded,transferred,encounters,shinies,berries,throws,xp,stardust,encounter,captured;
    uint64_t candidate_request;
    double latitude,longitude;
    uint32_t mode,state,error,reason,flags,pid,candidate_state,rewards_available;
    uint32_t visual_available,visual_failed,research_status,location_status,tap_request,farmer_activity;
    uint64_t iv_reads,name_updates;
} SilphSessionSnapshot;
uint32_t silph_wire_crc(const void *,size_t);
int silph_config_validate(const SilphSessionConfig *);
int silph_config_encode(const SilphSessionConfig *,void *,size_t);
int silph_config_decode(const void *,size_t,SilphSessionConfig *);
int silph_snapshot_encode(const SilphSessionSnapshot *,void *,size_t);
int silph_snapshot_decode(const void *,size_t,SilphSessionSnapshot *);
#endif
