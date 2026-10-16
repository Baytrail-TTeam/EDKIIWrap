#ifndef BAYTRAIL_M3_HANDOFF_H
#define BAYTRAIL_M3_HANDOFF_H
#include "Handoff.h"
#define M3_MAGIC UINT64_C(0x33464f484c545942)
#define M3_VERSION 3
#define M3_REALMODE_ARENA_SIZE (64u*1024u)
#define M3_LOW_ARENA_SIZE (4u*1024u*1024u)
#ifndef M3_ARENA_SIZE
#define M3_ARENA_SIZE (64u*1024u*1024u)
#endif
#pragma pack(push,1)
typedef struct {
 Handoff common;
 uint64_t m3_magic;
 uint32_t m3_version, m3_size;
 uint64_t arena_base, arena_size, fv_base, fv_size;
 uint64_t entry_base, entry_size;
 uint64_t firmware_system_table;
 uint64_t low_arena_base, low_arena_size;
 uint64_t realmode_arena_base, realmode_arena_size;
} M3Handoff;
#pragma pack(pop)
_Static_assert(sizeof(M3Handoff)==2416,"M3 ABI");
#endif
