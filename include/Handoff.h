#ifndef BAYTRAIL_HANDOFF_H
#define BAYTRAIL_HANDOFF_H
#include <stdint.h>
#define HANDOFF_MAGIC UINT64_C(0x31464f484c545942)
#define MAX_TABLES 128
#pragma pack(push, 1)
typedef struct { uint64_t address; uint32_t length, signature; } TableRef;
typedef struct {
 uint64_t magic;
 uint32_t version, size;
 uint64_t memory_map, memory_map_size, descriptor_size;
 uint32_t descriptor_version, table_count;
 uint64_t framebuffer, framebuffer_size;
 uint32_t width, height, stride, pixel_format;
 uint32_t red_mask, green_mask, blue_mask, reserved_mask;
 uint64_t acpi1, acpi2, smbios2, smbios3;
 uint64_t smbios2_data, smbios3_data;
 uint32_t smbios2_size, smbios3_size;
 uint64_t loader_base, loader_size, transition_base, transition_size;
 uint64_t payload_base, payload_size, stack_base, stack_size, page_tables;
 uint32_t cpuid_signature, flags;
 uint64_t initial_cr0, initial_cr4, initial_efer, initial_pat;
 TableRef tables[MAX_TABLES];
} Handoff;
#pragma pack(pop)
_Static_assert(sizeof(TableRef)==16, "table ABI");
_Static_assert(sizeof(Handoff)==2312, "handoff ABI");
#endif
