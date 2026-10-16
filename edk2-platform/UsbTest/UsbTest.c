#include <PiDxe.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/PciIo.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/HobLib.h>
#include "M3Handoff.h"
#include "Marker.h"
STATIC UINT8 MapBuffer[32768];
STATIC EFI_STATUS Envelope(UINT64 *Low,UINT64 *High) {
 UINTN Size=sizeof(MapBuffer),Key,Stride;UINT32 Version;EFI_STATUS S;
 S=gBS->GetMemoryMap(&Size,(VOID *)MapBuffer,&Key,&Stride,&Version);
 if(EFI_ERROR(S)) return S;
 *Low=MAX_UINT64;*High=0;
 for(UINTN I=0;I<Size;I+=Stride) {
  EFI_MEMORY_DESCRIPTOR *D=(VOID *)(MapBuffer+I);
  if(D->Type==EfiConventionalMemory) {
   if(D->PhysicalStart<*Low) *Low=D->PhysicalStart;
   if(D->PhysicalStart+(D->NumberOfPages<<12)>*High) *High=D->PhysicalStart+(D->NumberOfPages<<12);
  }
 }
 return *High?EFI_SUCCESS:EFI_NOT_FOUND;
}
STATIC EFI_STATUS CheckReserved(VOID) {
 UINT64 BeforeLow,BeforeHigh,AfterLow,AfterHigh;EFI_PHYSICAL_ADDRESS Address=0x1000;EFI_STATUS S,Request;
 S=Envelope(&BeforeLow,&BeforeHigh);if(EFI_ERROR(S)) return S;
 /* These legacy low pages are outside both explicitly acquired arenas.
  * Never touch the requested pages even if old DXE incorrectly frees them. */
 Request=gBS->AllocatePages(AllocateAddress,EfiLoaderData,16,&Address);
 if(!EFI_ERROR(Request)) gBS->FreePages(Address,16);
 S=Envelope(&AfterLow,&AfterHigh);if(EFI_ERROR(S)) return S;
 if(!EFI_ERROR(Request) || AfterLow<BeforeLow || AfterHigh>BeforeHigh) return EFI_SECURITY_VIOLATION;
 return EFI_SUCCESS;
}
EFI_STATUS EFIAPI UsbTestEntry(EFI_HANDLE Image,EFI_SYSTEM_TABLE *St) {
 EFI_PHYSICAL_ADDRESS Address=0;EFI_STATUS S;volatile UINT64 *First,*Last;
 EFI_TIME Time;EFI_TIME_CAPABILITIES Caps;
 if(St->RuntimeServices->GetTime(NULL,NULL)!=EFI_INVALID_PARAMETER ||
    EFI_ERROR(St->RuntimeServices->GetTime(&Time,&Caps)) || Caps.Resolution!=1 ||
    Time.Year<1900 || Time.Year>9999 || Time.Month<1 || Time.Month>12 ||
    Time.Day<1 || Time.Day>31 || Time.Hour>23 || Time.Minute>59 || Time.Second>59)
  return EFI_DEVICE_ERROR;
 Print(L"USB_RTC_READ_OK %04u-%02u-%02u %02u:%02u:%02u\r\n",Time.Year,Time.Month,Time.Day,Time.Hour,Time.Minute,Time.Second);
 {
  EFI_LOADED_IMAGE_PROTOCOL *Loaded;EFI_PCI_IO_PROTOCOL *Pci;
  if(!EFI_ERROR(gBS->HandleProtocol(Image,&gEfiLoadedImageProtocolGuid,(VOID **)&Loaded)) &&
     Loaded->LoadOptions && Loaded->LoadOptionsSize>=sizeof(L"exit-boot-services") &&
     StrStr(Loaded->LoadOptions,L"exit-boot-services")) {
   VOID *Hob=GetFirstGuidHob(&gBaytrailHandoffGuid);Handoff H;UINTN Size,Key,Stride;UINT32 Version;
   if(!Hob || GET_GUID_HOB_DATA_SIZE(Hob)!=sizeof(M3Handoff)) return EFI_NOT_FOUND;
   H=((M3Handoff *)GET_GUID_HOB_DATA(Hob))->common;
   Print(L"USB_EXIT_BOOT_SERVICES_BEGIN\r\n");
   for(UINTN Attempt=0;Attempt<2;Attempt++) {
    Size=sizeof(MapBuffer);S=gBS->GetMemoryMap(&Size,(VOID *)MapBuffer,&Key,&Stride,&Version);
    if(EFI_ERROR(S)) return S;
    S=gBS->ExitBootServices(Image,Key);if(!EFI_ERROR(S)) {
     if(St->BootServices || GetInterruptState()) {
      BaytrailMarker(&H,7,"USB EXIT BOOT STATE INVALID",0xff4040);BaytrailHalt();
     }
     BaytrailMarker(&H,20,"USB EXIT BOOT SERVICES OK",0x40ff40);BaytrailHalt();
    }
   }
   BaytrailMarker(&H,7,"USB EXIT BOOT SERVICES FAILED",0xff4040);BaytrailHalt();
  }
  if(!EFI_ERROR(gBS->HandleProtocol(Image,&gEfiLoadedImageProtocolGuid,(VOID **)&Loaded)) &&
     Loaded->LoadOptions && Loaded->LoadOptionsSize>=sizeof(L"cpu-fault") &&
     StrStr(Loaded->LoadOptions,L"cpu-fault")) {
   volatile UINT8 *Unmapped=(VOID *)(UINTN)0x100000000ULL;
   (VOID)*Unmapped;return EFI_DEVICE_ERROR;
  }
  if(!EFI_ERROR(gBS->HandleProtocol(Image,&gEfiLoadedImageProtocolGuid,(VOID **)&Loaded)) &&
     Loaded->LoadOptions && Loaded->LoadOptionsSize>=sizeof(L"dma-map-failure") &&
     StrStr(Loaded->LoadOptions,L"dma-map-failure")) {
   UINTN Bytes=8;VOID *Mapping=NULL;
   S=gBS->LocateProtocol(&gEfiPciIoProtocolGuid,NULL,(VOID **)&Pci);
   if(EFI_ERROR(S)) return S;
   /* Validate rejection only: no transfer or dereference of this address. */
   return Pci->Map(Pci,EfiPciIoOperationBusMasterWrite,(VOID *)(UINTN)0x100000000ULL,&Bytes,&Address,&Mapping);
  }
 }

 S=CheckReserved();
 if(EFI_ERROR(S)) {Print(L"USB_RESERVED_MEMORY_FAILED: %r\r\n",S);return S;}
 Print(L"USB_RESERVED_MEMORY_OK\r\n");
 {
  VOID *Hob=GetFirstGuidHob(&gBaytrailHandoffGuid);M3Handoff *M;EFI_PHYSICAL_ADDRESS Low=0xffffff;
  if(!Hob || GET_GUID_HOB_DATA_SIZE(Hob)!=sizeof(*M)) return EFI_NOT_FOUND;
  M=GET_GUID_HOB_DATA(Hob);
  S=gBS->AllocatePages(AllocateMaxAddress,EfiLoaderData,16,&Low);
  if(EFI_ERROR(S)) {Print(L"USB_LOW_MEMORY_FAILED: %r\r\n",S);return S;}
  BOOLEAN Owned=Low>=M->low_arena_base && Low+65536<=M->low_arena_base+M->low_arena_size && Low+65536<=0x1000000;
  gBS->FreePages(Low,16);if(!Owned) return EFI_SECURITY_VIOLATION;
  Print(L"USB_LOW_MEMORY_64K_OK\r\n");
  Low=0x9ffff;
  S=gBS->AllocatePages(AllocateMaxAddress,EfiLoaderData,4,&Low);
  if(EFI_ERROR(S)) {Print(L"USB_REALMODE_MEMORY_FAILED: %r\r\n",S);return S;}
  Owned=Low>=M->realmode_arena_base && Low+16384<=M->realmode_arena_base+M->realmode_arena_size && Low+16384<=0xa0000;
  if(Owned) {
   *(volatile UINT64 *)(UINTN)Low=0x42545245414c4d33ULL;
   Owned=*(volatile UINT64 *)(UINTN)Low==0x42545245414c4d33ULL;
  }
  gBS->FreePages(Low,4);if(!Owned) return EFI_SECURITY_VIOLATION;
  Print(L"USB_REALMODE_MEMORY_16K_OK\r\n");
 }
 /* Verify substantial conventional memory is available to a child EFI app. */
 S=gBS->AllocatePages(AllocateAnyPages,EfiLoaderData,640*256,&Address);
 if(EFI_ERROR(S)) {Print(L"USB_MEMORY_640M_FAILED: %r\r\n",S);return S;}
 First=(VOID *)(UINTN)Address;Last=(VOID *)(UINTN)(Address+640*1024*1024-8);
 *First=0x42544d454d313630ULL;*Last=0x42544d454d313630ULL;
 if(*First!=0x42544d454d313630ULL || *Last!=0x42544d454d313630ULL) S=EFI_DEVICE_ERROR;
 gBS->FreePages(Address,640*256);
 if(EFI_ERROR(S)) return S;
 Print(L"USB_MEMORY_640M_OK\r\n");Print(L"USB_X64_APP_OK: EDK2 loaded this X64 application from USB.\r\n");return EFI_SUCCESS;
}
