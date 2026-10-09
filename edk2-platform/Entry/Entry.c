#include <PiPei.h>
#include <Library/BaseLib.h>
#include "../../third_party/edk2/UefiPayloadPkg/UefiPayloadEntry/UefiPayloadEntry.h"
#include <Library/BaseMemoryLib.h>
#include <Library/HobLib.h>
#include <Library/PayloadEntryHelperLib.h>
#include <Guid/GraphicsInfoHob.h>
#include <UniversalPayload/AcpiTable.h>
#include <UniversalPayload/SmbiosTable.h>
#include "M3Handoff.h"
#include "Marker.h"
extern EFI_GUID gBaytrailHandoffGuid;
extern VOID *mHobList;
VOID EFIAPI ProcessLibraryConstructorList(VOID);
EFI_STATUS UniversalLoadDxeCore(EFI_FIRMWARE_VOLUME_HEADER *,EFI_PHYSICAL_ADDRESS *);
STATIC VOID Fail(CONST Handoff *H,CONST CHAR8 *Text) {BaytrailMarker(H,3,Text,0xff4040);BaytrailHalt();}
STATIC VOID Reserved(UINT64 Base,UINT64 Size,UINT32 Type) {
 if(!Size) return;
 /* PRESENT + INITIALIZED without TESTED is promotable by DXE after an
  * allocation failure. Stock reserved ranges must never become free RAM. */
 BuildResourceDescriptorHob(Type,EFI_RESOURCE_ATTRIBUTE_PRESENT|EFI_RESOURCE_ATTRIBUTE_INITIALIZED|EFI_RESOURCE_ATTRIBUTE_TESTED,Base,Size);
}
STATIC BOOLEAN BuildResources(M3Handoff *M) {
 Handoff *H=&M->common;UINT64 I,Base,Size,End,A=M->arena_base,Z=A+M->arena_size;
 BOOLEAN Found=FALSE;
 for(I=0;I<H->memory_map_size;I+=H->descriptor_size) {
  EFI_MEMORY_DESCRIPTOR *D=(VOID *)(UINTN)(H->memory_map+I);
  Base=D->PhysicalStart;
  if(D->NumberOfPages>(MAX_UINT64>>12)) return FALSE;
  Size=D->NumberOfPages<<12;
  if(Size>MAX_UINT64-Base) return FALSE;
  End=Base+Size;
  if(Base<Z && End>A) {
   if(Found || Base>A || End<Z || D->Type!=EfiLoaderData || !(D->Attribute&EFI_MEMORY_WB)) return FALSE;
   Found=TRUE;
   Reserved(Base,A-Base,EFI_RESOURCE_MEMORY_RESERVED);
   BuildResourceDescriptorHob(EFI_RESOURCE_SYSTEM_MEMORY,
    EFI_RESOURCE_ATTRIBUTE_PRESENT|EFI_RESOURCE_ATTRIBUTE_INITIALIZED|
    EFI_RESOURCE_ATTRIBUTE_TESTED|EFI_RESOURCE_ATTRIBUTE_WRITE_BACK_CACHEABLE,A,M->arena_size);
   Reserved(Z,End-Z,EFI_RESOURCE_MEMORY_RESERVED);
  } else {
   UINT32 T=(D->Type==EfiMemoryMappedIO)?EFI_RESOURCE_MEMORY_MAPPED_IO:EFI_RESOURCE_MEMORY_RESERVED;
   if(D->Type==EfiMemoryMappedIOPortSpace) continue;
   Reserved(Base,Size,T);
  }
  /* Preserve published ACPI/NVS/runtime types; never free these ranges. */
  if(D->Type==EfiACPIReclaimMemory || D->Type==EfiACPIMemoryNVS ||
     D->Type==EfiRuntimeServicesCode || D->Type==EfiRuntimeServicesData)
   BuildMemoryAllocationHob(Base,Size,(EFI_MEMORY_TYPE)D->Type);
 }
 return Found;
}
EFI_STATUS EFIAPI _ModuleEntryPoint(UINTN Parameter) {
 M3Handoff *M=(VOID *)Parameter;Handoff *H=&M->common;
 EFI_STATUS Status;EFI_PHYSICAL_ADDRESS Entry;
 UINT32 A,B,C,D;EFI_PEI_GRAPHICS_INFO_HOB *G;
 if(H->magic!=HANDOFF_MAGIC || H->version!=1 || H->size!=sizeof(*H)) BaytrailHalt();
 BaytrailMarker(H,0,"M3 X64 ENTRY",0xffffff);
 if(M->m3_magic!=M3_MAGIC || M->m3_version!=1 || M->m3_size!=sizeof(*M) ||
    M->arena_size!=M3_ARENA_SIZE || M->arena_base>=0x100000000ULL ||
    M->arena_base+M->arena_size>0x100000000ULL ||
    !H->descriptor_size || H->descriptor_size<sizeof(EFI_MEMORY_DESCRIPTOR) ||
    H->memory_map_size%H->descriptor_size || H->memory_map_size>65536)
  Fail(H,"M3 BAD HANDOFF");
 InitializeFloatingPointUnits();
 /* Official payload-entry HobLib/allocator run only inside our preallocated arena. */
 HobConstructor((VOID *)(UINTN)M->arena_base,(VOID *)(UINTN)(M->arena_base+M->arena_size),
                (VOID *)(UINTN)M->arena_base,(VOID *)(UINTN)(M->arena_base+M->arena_size));
 ProcessLibraryConstructorList();
 if(!BuildResources(M)) Fail(H,"M3 BAD MEMORY MAP");
 AsmCpuid(0x80000008,&A,&B,&C,&D);BuildCpuHob((UINT8)(A&255),16);
 BuildStackHob(H->stack_base,H->stack_size);
 BuildGuidDataHob(&gBaytrailHandoffGuid,M,sizeof(*M));
 G=BuildGuidHob(&gEfiGraphicsInfoHobGuid,sizeof(*G));
 G->FrameBufferBase=H->framebuffer;G->FrameBufferSize=(UINT32)H->framebuffer_size;
 G->GraphicsMode.Version=0;G->GraphicsMode.HorizontalResolution=H->width;
 G->GraphicsMode.VerticalResolution=H->height;G->GraphicsMode.PixelsPerScanLine=H->stride;
 G->GraphicsMode.PixelFormat=(EFI_GRAPHICS_PIXEL_FORMAT)H->pixel_format;
 G->GraphicsMode.PixelInformation.RedMask=H->red_mask;G->GraphicsMode.PixelInformation.GreenMask=H->green_mask;
 G->GraphicsMode.PixelInformation.BlueMask=H->blue_mask;G->GraphicsMode.PixelInformation.ReservedMask=H->reserved_mask;
 if(H->acpi2 || H->acpi1) {
  UNIVERSAL_PAYLOAD_ACPI_TABLE *T=BuildGuidHob(&gUniversalPayloadAcpiTableGuid,sizeof(*T));
  T->Header.Revision=UNIVERSAL_PAYLOAD_ACPI_TABLE_REVISION;T->Header.Length=sizeof(*T);
  T->Rsdp=H->acpi2?H->acpi2:H->acpi1;
 }
 if(H->smbios2) {
  UNIVERSAL_PAYLOAD_SMBIOS_TABLE *T=BuildGuidHob(&gUniversalPayloadSmbiosTableGuid,sizeof(*T));
  T->Header.Revision=UNIVERSAL_PAYLOAD_SMBIOS_TABLE_REVISION;T->Header.Length=sizeof(*T);T->SmBiosEntryPoint=H->smbios2;
 }
 if(H->smbios3) {
  UNIVERSAL_PAYLOAD_SMBIOS_TABLE *T=BuildGuidHob(&gUniversalPayloadSmbios3TableGuid,sizeof(*T));
  T->Header.Revision=UNIVERSAL_PAYLOAD_SMBIOS_TABLE_REVISION;T->Header.Length=sizeof(*T);T->SmBiosEntryPoint=H->smbios3;
 }
 BuildFvHob(M->fv_base,M->fv_size);
 BaytrailMarker(H,1,"M3 HOBS READY",0xffffff);
 BaytrailMarker(H,2,"M3 LOADING DXE CORE",0xffffff);
 Status=UniversalLoadDxeCore((VOID *)(UINTN)M->fv_base,&Entry);
 if(EFI_ERROR(Status)) Fail(H,"M3 DXE LOAD FAILED");
 BaytrailMarker(H,3,"M3 ENTERING DXE CORE",0xffffff);
 /* Keep tested stack and CR3. No legacy PIC, APIC, GPIO or controller writes. */
 ((VOID(EFIAPI *)(VOID *))(UINTN)Entry)(mHobList);
 Fail(H,"M3 DXE RETURNED");return EFI_ABORTED;
}
