#include <Uefi.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/PciIo.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
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
 UINT64 BeforeLow,BeforeHigh,AfterLow,AfterHigh;EFI_PHYSICAL_ADDRESS Address=0x102000;EFI_STATUS S,Request;
 S=Envelope(&BeforeLow,&BeforeHigh);if(EFI_ERROR(S)) return S;
 /* Mirror the failed low allocation seen in the Windows call stack.
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
 /* Verify substantial conventional memory is available to a child EFI app. */
 S=gBS->AllocatePages(AllocateAnyPages,EfiLoaderData,160*256,&Address);
 if(EFI_ERROR(S)) {Print(L"USB_MEMORY_160M_FAILED: %r\r\n",S);return S;}
 First=(VOID *)(UINTN)Address;Last=(VOID *)(UINTN)(Address+160*1024*1024-8);
 *First=0x42544d454d313630ULL;*Last=0x42544d454d313630ULL;
 if(*First!=0x42544d454d313630ULL || *Last!=0x42544d454d313630ULL) S=EFI_DEVICE_ERROR;
 gBS->FreePages(Address,160*256);
 if(EFI_ERROR(S)) return S;
 Print(L"USB_MEMORY_160M_OK\r\n");Print(L"USB_X64_APP_OK: EDK2 loaded this X64 application from USB.\r\n");return EFI_SUCCESS;
}
