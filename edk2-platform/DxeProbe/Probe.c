#include <PiDxe.h>
#include <Library/HobLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Guid/Acpi.h>
#include <Guid/SmBios.h>
#include "M3Handoff.h"
#include "Marker.h"
extern EFI_GUID gBaytrailHandoffGuid;
STATIC EFI_GUID ProbeGuid={0x7b4d789e,0x2044,0x4307,{0x94,0x17,0x5f,0x3e,0x11,0x6f,0x6b,0x49}};
STATIC VOID Fail(CONST Handoff *H,CONST CHAR8 *Text) {BaytrailMarker(H,4,Text,0xff4040);BaytrailHalt();}
STATIC BOOLEAN Published(EFI_SYSTEM_TABLE *St,EFI_GUID *Guid,UINT64 Address) {
 UINTN I;for(I=0;I<St->NumberOfTableEntries;I++)
  if(CompareGuid(&St->ConfigurationTable[I].VendorGuid,Guid)) return (UINTN)St->ConfigurationTable[I].VendorTable==Address;
 return FALSE;
}
EFI_STATUS EFIAPI ProbeEntry(EFI_HANDLE Image,EFI_SYSTEM_TABLE *St) {
 VOID *GuidHob=GetFirstGuidHob(&gBaytrailHandoffGuid),*Buffer,*Found;
 M3Handoff *M;Handoff *H;EFI_STATUS Status;EFI_PHYSICAL_ADDRESS Page=0;
 EFI_HANDLE Handle=NULL;UINT32 Token=0x33444b45,Crc;UINTN Size=0,Key,Ds,Capacity;UINT32 Dv;
 UINT64 I;
 (VOID)Image;
 if(!GuidHob || GET_GUID_HOB_DATA_SIZE(GuidHob)!=sizeof(*M)) BaytrailHalt();
 M=GET_GUID_HOB_DATA(GuidHob);H=&M->common;
 BaytrailMarker(H,4,"M3 DXE DRIVER STARTED",0xffffff);
 if(St->Hdr.Signature!=EFI_SYSTEM_TABLE_SIGNATURE || (UINTN)St==M->firmware_system_table || gBS!=St->BootServices)
  Fail(H,"M3 SYSTEM TABLE FAILED");
 Status=gBS->CalculateCrc32("123456789",9,&Crc);
 if(EFI_ERROR(Status)||Crc!=0xcbf43926) Fail(H,"M3 CRC FAILED");
 Status=gBS->AllocatePages(AllocateAnyPages,EfiBootServicesData,1,&Page);
 if(EFI_ERROR(Status)||Page<M->arena_base || Page+4096>M->arena_base+M->arena_size) Fail(H,"M3 PAGE ALLOCATION FAILED");
 *(volatile UINT64 *)(UINTN)Page=M3_MAGIC;
 if(*(volatile UINT64 *)(UINTN)Page!=M3_MAGIC || EFI_ERROR(gBS->FreePages(Page,1))) Fail(H,"M3 PAGE TEST FAILED");
 Status=gBS->InstallProtocolInterface(&Handle,&ProbeGuid,EFI_NATIVE_INTERFACE,&Token);
 if(EFI_ERROR(Status)) Fail(H,"M3 PROTOCOL INSTALL FAILED");
 Status=gBS->LocateProtocol(&ProbeGuid,NULL,&Found);
 if(EFI_ERROR(Status)||Found!=&Token) Fail(H,"M3 PROTOCOL LOOKUP FAILED");
 if(EFI_ERROR(gBS->UninstallProtocolInterface(Handle,&ProbeGuid,&Token))) Fail(H,"M3 PROTOCOL REMOVE FAILED");
 if(H->acpi2 && EFI_ERROR(gBS->InstallConfigurationTable(&gEfiAcpiTableGuid,(VOID *)(UINTN)H->acpi2))) Fail(H,"M3 ACPI INSTALL FAILED");
 if(H->acpi1 && EFI_ERROR(gBS->InstallConfigurationTable(&gEfiAcpi10TableGuid,(VOID *)(UINTN)H->acpi1))) Fail(H,"M3 ACPI INSTALL FAILED");
 if(H->smbios2 && EFI_ERROR(gBS->InstallConfigurationTable(&gEfiSmbiosTableGuid,(VOID *)(UINTN)H->smbios2))) Fail(H,"M3 SMBIOS INSTALL FAILED");
 if(H->smbios3 && EFI_ERROR(gBS->InstallConfigurationTable(&gEfiSmbios3TableGuid,(VOID *)(UINTN)H->smbios3))) Fail(H,"M3 SMBIOS INSTALL FAILED");
 if((H->acpi2&&!Published(St,&gEfiAcpiTableGuid,H->acpi2)) || (H->acpi1&&!Published(St,&gEfiAcpi10TableGuid,H->acpi1)) ||
    (H->smbios2&&!Published(St,&gEfiSmbiosTableGuid,H->smbios2)) || (H->smbios3&&!Published(St,&gEfiSmbios3TableGuid,H->smbios3))) Fail(H,"M3 TABLE PASSTHROUGH FAILED");
 Status=gBS->GetMemoryMap(&Size,NULL,&Key,&Ds,&Dv);
 if(Status!=EFI_BUFFER_TOO_SMALL || Ds<sizeof(EFI_MEMORY_DESCRIPTOR)) Fail(H,"M3 MEMORY MAP QUERY FAILED");
 Capacity=Size+16*Ds;
 if(EFI_ERROR(gBS->AllocatePool(EfiBootServicesData,Capacity,&Buffer))) Fail(H,"M3 POOL ALLOCATION FAILED");
 Size=Capacity;Status=gBS->GetMemoryMap(&Size,Buffer,&Key,&Ds,&Dv);
 if(EFI_ERROR(Status)||!Ds||Size%Ds) Fail(H,"M3 MEMORY MAP FAILED");
 for(I=0;I<Size;I+=Ds) {
  EFI_MEMORY_DESCRIPTOR *D=(VOID *)((UINT8 *)Buffer+I);
  if(D->Type==EfiConventionalMemory && (D->PhysicalStart<M->arena_base ||
    D->NumberOfPages>((M->arena_base+M->arena_size-D->PhysicalStart)>>12) ||
    D->PhysicalStart>=M->arena_base+M->arena_size)) Fail(H,"M3 STOCK MEMORY RELEASED");
 }
 if(EFI_ERROR(gBS->FreePool(Buffer))) Fail(H,"M3 POOL FREE FAILED");
 BaytrailMarker(H,4,"M3 EDK2 DXE OK",0x40ff40);
 BaytrailMarker(H,5,"BOOT SERVICES TEST OK",0x40ff40);
 BaytrailMarker(H,6,"ACPI SMBIOS TABLES OK",0x40ff40);
 BaytrailHalt();return EFI_SUCCESS;
}
