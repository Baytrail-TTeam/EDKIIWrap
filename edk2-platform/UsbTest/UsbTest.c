#include <Uefi.h>
#include <Library/UefiLib.h>
EFI_STATUS EFIAPI UsbTestEntry(EFI_HANDLE Image,EFI_SYSTEM_TABLE *St) {
 (VOID)Image;(VOID)St;Print(L"USB_X64_APP_OK: EDK2 loaded this X64 application from USB.\r\n");return EFI_SUCCESS;
}
