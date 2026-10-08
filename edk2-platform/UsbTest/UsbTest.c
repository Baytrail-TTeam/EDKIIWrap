#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
EFI_STATUS EFIAPI UsbTestEntry(EFI_HANDLE Image,EFI_SYSTEM_TABLE *St) {
 EFI_PHYSICAL_ADDRESS Address=0;EFI_STATUS S;volatile UINT64 *First,*Last;
 (VOID)Image;(VOID)St;
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
