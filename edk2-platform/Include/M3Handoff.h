#ifndef BAYTRAIL_M3_HANDOFF_H
#define BAYTRAIL_M3_HANDOFF_H
#include "Handoff.h"
#define M3_MAGIC UINT64_C(0x33464f484c545942)
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
} M3Handoff;
#pragma pack(pop)
_Static_assert(sizeof(M3Handoff)==2384,"M3 ABI");
#endif
