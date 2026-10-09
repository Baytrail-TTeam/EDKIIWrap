/* Native xHCI USB bootstrap; RAM FAT is retained only for Shell. */
#define BAYTRAIL_PROBE_CONTINUE
#define ProbeEntry M3Checks
#include "../DxeProbe/Probe.c"
#undef ProbeEntry
#include <Protocol/GraphicsOutput.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/SimpleTextOut.h>
#include <Protocol/BlockIo.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/LoadedImage.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DxeServicesLib.h>
#include <Library/DevicePathLib.h>
#include <Guid/IdleLoopEvent.h>
#include <Protocol/UsbIo.h>
#include <Library/PrintLib.h>
#include "UsbTrace.h"
#include <Protocol/Usb2HostController.h>
#include <Protocol/PciIo.h>
#include <Protocol/SimpleTextInEx.h>
#include <Library/UefiLib.h>
#include <Library/TimerLib.h>
#include <Library/BaseLib.h>
STATIC EFI_GUID DiskGuid={0x9e4bcd6e,0x3c20,0x4d90,{0xa0,0xb8,0xf2,0x17,0x18,0x10,0x5c,0x93}};
STATIC UINT8 *Disk;STATIC UINTN DiskBytes;
STATIC EFI_BLOCK_IO_MEDIA Media={.MediaId=1,.MediaPresent=TRUE,.LogicalPartition=TRUE,
 .ReadOnly=TRUE,.BlockSize=512,.IoAlign=1,.LastBlock=8191};
STATIC EFI_BLOCK_IO_PROTOCOL Block;
STATIC EFI_HANDLE DiskHandle;
STATIC struct {VENDOR_DEVICE_PATH Vendor;EFI_DEVICE_PATH_PROTOCOL End;} DiskPath={
 {{HARDWARE_DEVICE_PATH,HW_VENDOR_DP,{sizeof(VENDOR_DEVICE_PATH),0}},
  {0x9e4bcd6e,0x3c20,0x4d90,{0xa0,0xb8,0xf2,0x17,0x18,0x10,0x5c,0x93}}},
 {END_DEVICE_PATH_TYPE,END_ENTIRE_DEVICE_PATH_SUBTYPE,{sizeof(EFI_DEVICE_PATH_PROTOCOL),0}}};
STATIC EFI_STATUS EFIAPI ResetDisk(EFI_BLOCK_IO_PROTOCOL *This,BOOLEAN Extended) {(VOID)This;(VOID)Extended;return EFI_SUCCESS;}
STATIC EFI_STATUS EFIAPI ReadDisk(EFI_BLOCK_IO_PROTOCOL *This,UINT32 Id,EFI_LBA Lba,UINTN Size,VOID *Buffer) {
 UINT64 Offset;UINTN CopySize;
 if(This!=&Block) return EFI_INVALID_PARAMETER;
 if(Id!=Media.MediaId) return EFI_MEDIA_CHANGED;
 if(!Size) return EFI_SUCCESS;
 if(Size%512) return EFI_BAD_BUFFER_SIZE;
 if(!Buffer || Lba>Media.LastBlock || Size/512>Media.LastBlock+1-Lba) return EFI_INVALID_PARAMETER;
 Offset=Lba*512;ZeroMem(Buffer,Size);
 if(Offset<DiskBytes) {CopySize=MIN(Size,DiskBytes-(UINTN)Offset);CopyMem(Buffer,Disk+(UINTN)Offset,CopySize);}
 return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI WriteDisk(EFI_BLOCK_IO_PROTOCOL *This,UINT32 Id,EFI_LBA Lba,UINTN Size,VOID *Buffer) {
 (VOID)This;(VOID)Id;(VOID)Lba;(VOID)Size;(VOID)Buffer;return EFI_WRITE_PROTECTED;
}
STATIC EFI_STATUS EFIAPI FlushDisk(EFI_BLOCK_IO_PROTOCOL *This) {(VOID)This;return EFI_SUCCESS;}
STATIC Handoff *ContextH;
EFI_STATUS InstallBootCpu(VOID);
STATIC EFI_EXIT_BOOT_SERVICES OriginalExit;
STATIC EFI_STATUS EFIAPI ObserveExit(EFI_HANDLE Image,UINTN Key) {
 EFI_STATUS S;CHAR8 Text[80];
 BaytrailMarker(ContextH,4,"EFI EXIT BOOT SERVICES REQUEST",0xffa040);
 S=OriginalExit(Image,Key);
 /* After success the Boot Services table has been cleared: framebuffer only. */
 AsciiSPrint(Text,sizeof(Text),"EFI EXIT BOOT SERVICES STATUS %LX",(UINT64)S);
 BaytrailMarker(ContextH,4,Text,EFI_ERROR(S)?0xffa040:0x40ff40);return S;
}
STATIC BOOLEAN ScriptOk,KeyboardCommandOk,StorageOk,ManualAppOk;
STATIC CHAR16 Trace[32768];STATIC UINTN TraceLength;
STATIC EFI_TEXT_STRING OriginalOutput;
STATIC EFI_STATUS EFIAPI ObserveOutput(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,CHAR16 *Text) {
 UINTN I,J;CONST CHAR16 Target[]=L"SHELL_X64_RAM_OK";
 EFI_STATUS S=OriginalOutput(This,Text);
 for(I=0;Text[I] && TraceLength+1<ARRAY_SIZE(Trace);I++) Trace[TraceLength++]=Text[I];
 Trace[TraceLength]=0;
 for(I=0;I+ARRAY_SIZE(Target)-1<=TraceLength;I++) {
  for(J=0;J<ARRAY_SIZE(Target)-1 && Trace[I+J]==Target[J];J++) {}
  if(J==ARRAY_SIZE(Target)-1) ScriptOk=TRUE;
 }
 {
  CONST CHAR16 KeyToken[]=L"\r\nusb_key_ok\r\n";
  for(I=0;I+ARRAY_SIZE(KeyToken)-1<=TraceLength;I++) {
   for(J=0;J<ARRAY_SIZE(KeyToken)-1 && Trace[I+J]==KeyToken[J];J++) {}
   if(J==ARRAY_SIZE(KeyToken)-1) KeyboardCommandOk=TRUE;
  }
 }
 if(StrStr(Trace,L"USB_X64_APP_OK: EDK2 loaded this X64 application from USB.")) ManualAppOk=TRUE;
 return S;
}
STATIC BOOLEAN ReadyDrawn,KeyDrawn,AppDrawn;
STATIC VOID EFIAPI Idle(EFI_EVENT Event,VOID *Context) {
 (VOID)Event;(VOID)Context;
 if(ManualAppOk && !AppDrawn) {BaytrailMarker(ContextH,20,"USB MANUAL X64 APP OK",0x40ff40);AppDrawn=TRUE;}
 if(KeyboardCommandOk && !KeyDrawn) {
  BaytrailMarker(ContextH,22,"USB KEYBOARD COMMAND OK",0x40ff40);KeyDrawn=TRUE;
 }
 if(ScriptOk && !ReadyDrawn) {
  if(StorageOk) BaytrailMarker(ContextH,21,"USB FILESYSTEM READY",0x40ff40);
  BaytrailMarker(ContextH,23,"USB SHELL KEYBOARD READY",0x40ff40);ReadyDrawn=TRUE;
 }
}
STATIC VOID Check(EFI_STATUS Status,CONST CHAR8 *Message) {
 if(EFI_ERROR(Status)) {BaytrailMarker(ContextH,7,Message,0xff4040);BaytrailHalt();}
}
STATIC BOOLEAN HasUsbPath(EFI_HANDLE Handle) {
 EFI_DEVICE_PATH_PROTOCOL *Path;
 if(EFI_ERROR(gBS->HandleProtocol(Handle,&gEfiDevicePathProtocolGuid,(VOID **)&Path))) return FALSE;
 while(!IsDevicePathEnd(Path)) {
  if(DevicePathType(Path)==MESSAGING_DEVICE_PATH && DevicePathSubType(Path)==MSG_USB_DP) return TRUE;
  Path=NextDevicePathNode(Path);
 }
 return FALSE;
}
/* During bootstrap connect each published interface once, rather than
 * recursively re-probing the same disks every second. No hotplug reuse. */
STATIC EFI_HANDLE SeenUsb[128],SeenBlock[128];STATIC UINTN UsbCount,BlockCount;
STATIC VOID ConnectProtocol(EFI_GUID *Guid) {
 EFI_HANDLE *Handles,*Seen=NULL;UINTN Count,I,J,*SeenCount=NULL;
 if(Guid==&gEfiUsbIoProtocolGuid) {Seen=SeenUsb;SeenCount=&UsbCount;}
 if(Guid==&gEfiBlockIoProtocolGuid) {Seen=SeenBlock;SeenCount=&BlockCount;}
 if(EFI_ERROR(gBS->LocateHandleBuffer(ByProtocol,Guid,NULL,&Count,&Handles))) return;
 for(I=0;I<Count;I++) {
  if(Seen) {
   for(J=0;J<*SeenCount && Seen[J]!=Handles[I];J++) {}
   if(J<*SeenCount) continue;
   Check(*SeenCount<128?EFI_SUCCESS:EFI_OUT_OF_RESOURCES,"USB HANDLE LIMIT");
   Seen[(*SeenCount)++]=Handles[I];
  }
  gBS->ConnectController(Handles[I],NULL,NULL,TRUE);
 }
 FreePool(Handles);
}
STATIC BOOLEAN FindUsbVolume(VOID) {
 EFI_HANDLE *Handles;UINTN Count,I;BOOLEAN Found=FALSE;
 if(EFI_ERROR(gBS->LocateHandleBuffer(ByProtocol,&gEfiSimpleFileSystemProtocolGuid,NULL,&Count,&Handles))) return FALSE;
 for(I=0;I<Count;I++) if(HasUsbPath(Handles[I])) {Found=TRUE;break;}
 FreePool(Handles);return Found;
}
STATIC BOOLEAN UseUsbKeyboard(EFI_SYSTEM_TABLE *St) {
 EFI_HANDLE *Handles;UINTN Count,I;EFI_SIMPLE_TEXT_INPUT_PROTOCOL *Keyboard;BOOLEAN Found=FALSE;
 if(EFI_ERROR(gBS->LocateHandleBuffer(ByProtocol,&gEfiSimpleTextInProtocolGuid,NULL,&Count,&Handles))) return FALSE;
 for(I=0;I<Count;I++) if(HasUsbPath(Handles[I]) &&
    !EFI_ERROR(gBS->HandleProtocol(Handles[I],&gEfiSimpleTextInProtocolGuid,(VOID **)&Keyboard))) {
  St->ConsoleInHandle=Handles[I];St->ConIn=Keyboard;Found=TRUE;break;
 }
 FreePool(Handles);return Found;
}
STATIC VOID NativeUsb(EFI_SYSTEM_TABLE *St) {
 UINTN Round;EFI_USB2_HC_PROTOCOL *Hc;BOOLEAN Keyboard=FALSE;
 Print(L"USB: starting controller and discovering devices...\r\n");
 ConnectProtocol(&gEfiPciIoProtocolGuid);
 Check(gBS->LocateProtocol(&gEfiUsb2HcProtocolGuid,NULL,(VOID **)&Hc),"USB XHCI DRIVER FAILED");
 /* Exit as soon as FAT and keyboard exist; 15 s is only the discovery cap.
  * Driver ConnectController calls remain synchronous and may take longer. */
 for(Round=0;Round<750;Round++) {
  if(Round%5==0) {ConnectProtocol(&gEfiUsbIoProtocolGuid);ConnectProtocol(&gEfiBlockIoProtocolGuid);}
  StorageOk=FindUsbVolume();Keyboard=UseUsbKeyboard(St);
  if(StorageOk && Keyboard) break;
  gBS->Stall(20000);
 }
 if(!Keyboard) Check(EFI_NOT_FOUND,"USB KEYBOARD MISSING");
 Print(StorageOk?L"USB: filesystem and keyboard ready; entering Shell.\r\n":L"USB: keyboard ready; no USB filesystem yet. Entering Shell.\r\n");
}
EFI_STATUS EFIAPI UsbBootEntry(EFI_HANDLE Image,EFI_SYSTEM_TABLE *St) {
 VOID *Mh=GetFirstGuidHob(&gBaytrailHandoffGuid),*Fs;EFI_STATUS Status;
 EFI_HANDLE *Handles,App;UINTN Count,I;EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *Out=NULL;
 EFI_DEVICE_PATH_PROTOCOL *FilePath;EFI_LOADED_IMAGE_PROTOCOL *Loaded;EFI_EVENT IdleEvent;
 UINT32 Crc;M3Handoff *M;UINT8 Sector[512];
 if(!Mh || GET_GUID_HOB_DATA_SIZE(Mh)!=sizeof(*M)) BaytrailHalt();
 M=GET_GUID_HOB_DATA(Mh);ContextH=&M->common;
 Check(M3Checks(Image,St),"SHELL M3 CHECKS FAILED");
 BaytrailMarker(ContextH,7,"SHELL PREPARING CONSOLE",0xffffff);
 Check(gBS->LocateHandleBuffer(ByProtocol,&gEfiGraphicsOutputProtocolGuid,NULL,&Count,&Handles),"SHELL GOP HANDLE FAILED");
 for(I=0;I<Count;I++) {
  gBS->ConnectController(Handles[I],NULL,NULL,TRUE);
  if(!EFI_ERROR(gBS->HandleProtocol(Handles[I],&gEfiSimpleTextOutProtocolGuid,(VOID **)&Out))) break;
 }
 if(!Out) {FreePool(Handles);Check(EFI_NOT_FOUND,"SHELL TEXT CONSOLE MISSING");}
 St->ConsoleOutHandle=Handles[I];St->StandardErrorHandle=Handles[I];St->ConOut=Out;St->StdErr=Out;FreePool(Handles);
 NativeUsb(St);
 {
  BAYTRAIL_USB_TRACE_CONTROL *Control;
  Check(gBS->LocateProtocol(&gBaytrailUsbTraceControlGuid,NULL,(VOID **)&Control),"USB TRACE CONTROL MISSING");
  if(Control->Revision==1) Control->Enabled=FALSE;
 }

 Check(InstallBootCpu(),"CPU ARCH INSTALL FAILED");
 OriginalExit=gBS->ExitBootServices;gBS->ExitBootServices=ObserveExit;
 gBS->Hdr.CRC32=0;
 Check(gBS->CalculateCrc32(gBS,gBS->Hdr.HeaderSize,&gBS->Hdr.CRC32),"BOOT SERVICES CRC FAILED");
 St->FirmwareVendor=AllocateRuntimeCopyPool(sizeof(L"Baytrail EDK2 X64"),L"Baytrail EDK2 X64");
 if(!St->FirmwareVendor) Check(EFI_OUT_OF_RESOURCES,"SHELL VENDOR ALLOCATION FAILED");
 St->FirmwareRevision=0x00010000;
 St->Hdr.CRC32=0;Check(gBS->CalculateCrc32(St,St->Hdr.HeaderSize,&Crc),"SHELL ST CRC FAILED");St->Hdr.CRC32=Crc;
 Check(GetSectionFromAnyFv(&DiskGuid,EFI_SECTION_RAW,0,(VOID **)&Disk,&DiskBytes),"SHELL RAM DISK LOAD FAILED");
 if(DiskBytes<512 || DiskBytes>4194304 || Disk[510]!=0x55 || Disk[511]!=0xaa) Check(EFI_VOLUME_CORRUPTED,"SHELL RAM DISK INVALID");
 Block.Revision=EFI_BLOCK_IO_PROTOCOL_REVISION;Block.Media=&Media;Block.Reset=ResetDisk;
 Block.ReadBlocks=ReadDisk;Block.WriteBlocks=WriteDisk;Block.FlushBlocks=FlushDisk;
 Check(ReadDisk(&Block,1,0,sizeof(Sector),Sector),"SHELL DISK READ FAILED");
 if(Sector[510]!=0x55 || Sector[511]!=0xaa ||
    ReadDisk(&Block,2,0,sizeof(Sector),Sector)!=EFI_MEDIA_CHANGED ||
    ReadDisk(&Block,1,8192,sizeof(Sector),Sector)!=EFI_INVALID_PARAMETER ||
    ReadDisk(&Block,1,0,1,Sector)!=EFI_BAD_BUFFER_SIZE ||
    WriteDisk(&Block,1,0,sizeof(Sector),Sector)!=EFI_WRITE_PROTECTED)
  Check(EFI_DEVICE_ERROR,"SHELL DISK GUARD FAILED");
 Check(gBS->InstallMultipleProtocolInterfaces(&DiskHandle,&gEfiBlockIoProtocolGuid,&Block,&gEfiDevicePathProtocolGuid,&DiskPath,NULL),"SHELL DISK INSTALL FAILED");
 Check(gBS->ConnectController(DiskHandle,NULL,NULL,TRUE),"SHELL DISK CONNECT FAILED");
 Check(gBS->HandleProtocol(DiskHandle,&gEfiSimpleFileSystemProtocolGuid,&Fs),"SHELL FAT FS MISSING");
 FilePath=FileDevicePath(DiskHandle,L"\\EFI\\BOOT\\BOOTX64.EFI");
 if(!FilePath) Check(EFI_OUT_OF_RESOURCES,"SHELL FILE PATH FAILED");
 Check(gBS->LoadImage(FALSE,Image,FilePath,NULL,0,&App),"SHELL LOAD IMAGE FAILED");FreePool(FilePath);
 Check(gBS->HandleProtocol(App,&gEfiLoadedImageProtocolGuid,(VOID **)&Loaded),"SHELL LOADED IMAGE MISSING");
 Loaded->LoadOptions=L"-nointerrupt";Loaded->LoadOptionsSize=sizeof(L"-nointerrupt");
 /* Keep polling via UsbPlatform idle ticks; native keyboard remains live. */
 Check(gBS->CreateEventEx(EVT_NOTIFY_SIGNAL,TPL_CALLBACK,Idle,NULL,&gIdleLoopEventGuid,&IdleEvent),"SHELL IDLE EVENT FAILED");
 OriginalOutput=Out->OutputString;Out->OutputString=ObserveOutput;
 Out->ClearScreen(Out);Out->OutputString(Out,L"Baytrail: EDK2 X64 Shell / read-only RAM FAT\r\nUSB keyboard enabled. Choose the EFI file manually from a USB filesystem.\r\n\r\n");
 Status=gBS->StartImage(App,NULL,NULL);
 Out->OutputString(Out,L"\r\nShell returned. Test stopped.\r\n");
 Check(Status,"SHELL START IMAGE FAILED");BaytrailHalt();return EFI_SUCCESS;
}
