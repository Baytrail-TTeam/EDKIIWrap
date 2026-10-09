/* IRQ-disabled firmware must advance USB timers even when an application
 * polls ReadKeyStroke directly instead of waiting on WaitForKey. */
#include <Uefi.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/SimpleTextInEx.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DevicePathLib.h>

VOID PumpUsbTimer(VOID);
typedef struct {EFI_SIMPLE_TEXT_INPUT_PROTOCOL *Io;EFI_INPUT_READ_KEY Read;} INPUT_RECORD;
typedef struct {EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *Io;EFI_INPUT_READ_KEY_EX Read;} INPUT_EX_RECORD;
STATIC INPUT_RECORD Inputs[64];
STATIC INPUT_EX_RECORD InputsEx[64];
STATIC UINTN InputCount,InputExCount;
STATIC VOID *Registration,*RegistrationEx;

STATIC EFI_STATUS EFIAPI ReadKey(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This,EFI_INPUT_KEY *Key) {
 UINTN I;
 for(I=0;I<InputCount;I++) if(Inputs[I].Io==This) {
  PumpUsbTimer();return Inputs[I].Read(This,Key);
 }
 return EFI_DEVICE_ERROR;
}
STATIC EFI_STATUS EFIAPI ReadKeyEx(EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *This,EFI_KEY_DATA *Key) {
 UINTN I;
 for(I=0;I<InputExCount;I++) if(InputsEx[I].Io==This) {
  PumpUsbTimer();return InputsEx[I].Read(This,Key);
 }
 return EFI_DEVICE_ERROR;
}
STATIC BOOLEAN IsUsb(EFI_HANDLE Handle) {
 EFI_DEVICE_PATH_PROTOCOL *Dp=DevicePathFromHandle(Handle);
 if(!Dp) return FALSE;
 for(;!IsDevicePathEnd(Dp);Dp=NextDevicePathNode(Dp))
  if(DevicePathType(Dp)==MESSAGING_DEVICE_PATH && DevicePathSubType(Dp)==MSG_USB_DP) return TRUE;
 return FALSE;
}
STATIC VOID EFIAPI HookInput(EFI_EVENT Event,VOID *Context) {
 BOOLEAN Ex=(BOOLEAN)(UINTN)Context;VOID *Reg=Ex?RegistrationEx:Registration;
 EFI_GUID *Guid=Ex?&gEfiSimpleTextInputExProtocolGuid:&gEfiSimpleTextInProtocolGuid;
 EFI_HANDLE Handle;UINTN Size,I;VOID *Protocol;(VOID)Event;
 for(;;) {
  Size=sizeof(Handle);
  if(EFI_ERROR(gBS->LocateHandle(ByRegisterNotify,NULL,Reg,&Size,&Handle))) break;
  if(!IsUsb(Handle) || EFI_ERROR(gBS->HandleProtocol(Handle,Guid,&Protocol))) continue;
  if(Ex) {
   EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *Io=Protocol;
   /* Also handles reinstall/reuse without ever saving our own wrapper. */
   if(Io->ReadKeyStrokeEx==ReadKeyEx) continue;
   for(I=0;I<InputExCount && InputsEx[I].Io!=Io;I++) {}
   if(I==ARRAY_SIZE(InputsEx)) continue;
   if(I==InputExCount) InputExCount++;
   InputsEx[I].Io=Io;InputsEx[I].Read=Io->ReadKeyStrokeEx;Io->ReadKeyStrokeEx=ReadKeyEx;
  } else {
   EFI_SIMPLE_TEXT_INPUT_PROTOCOL *Io=Protocol;
   if(Io->ReadKeyStroke==ReadKey) continue;
   for(I=0;I<InputCount && Inputs[I].Io!=Io;I++) {}
   if(I==ARRAY_SIZE(Inputs)) continue;
   if(I==InputCount) InputCount++;
   Inputs[I].Io=Io;Inputs[I].Read=Io->ReadKeyStroke;Io->ReadKeyStroke=ReadKey;
  }
 }
}
EFI_STATUS HookUsbKeyboard(VOID) {
 EFI_EVENT Event;EFI_STATUS S;UINTN I;
 for(I=0;I<2;I++) {
  S=gBS->CreateEvent(EVT_NOTIFY_SIGNAL,TPL_NOTIFY,HookInput,(VOID *)I,&Event);
  if(EFI_ERROR(S)) return S;
  S=gBS->RegisterProtocolNotify(I?&gEfiSimpleTextInputExProtocolGuid:&gEfiSimpleTextInProtocolGuid,Event,I?&RegistrationEx:&Registration);
  if(EFI_ERROR(S)) {gBS->CloseEvent(Event);return S;}
  gBS->SignalEvent(Event);
 }
 return EFI_SUCCESS;
}
