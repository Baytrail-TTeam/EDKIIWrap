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
#include <Protocol/DriverBinding.h>
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
STATIC BOOLEAN ScriptOk,KeyboardCommandOk,StorageOk;
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
 return S;
}
STATIC BOOLEAN ReadyDrawn,KeyDrawn;
STATIC VOID EFIAPI Idle(EFI_EVENT Event,VOID *Context) {
 (VOID)Event;(VOID)Context;
 if(KeyboardCommandOk && !KeyDrawn) {
  BaytrailMarker(ContextH,22,"USB KEYBOARD COMMAND OK",0x40ff40);KeyDrawn=TRUE;
 }
 if(ScriptOk && !ReadyDrawn) {
  if(StorageOk) BaytrailMarker(ContextH,21,"USB STORAGE X64 APP OK",0x40ff40);
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
typedef struct {
 EFI_GUID Guid;CONST CHAR8 *Name;EFI_DRIVER_BINDING_PROTOCOL *Binding;
 EFI_DRIVER_BINDING_SUPPORTED Supported;EFI_DRIVER_BINDING_START Start;
} DISK_DRIVER;
STATIC BOOLEAN TraceBindings=TRUE;
STATIC DISK_DRIVER DiskDrivers[]={
 {{0x6b38f7b4,0xad98,0x40e9,{0x90,0x93,0xac,0xa2,0xb5,0xa2,0x53,0xc4}},"DISKIO",NULL,NULL,NULL},
 {{0x1fa1f39e,0xfeff,0x4aae,{0xbd,0x7b,0x38,0xa0,0x70,0xa3,0xb6,0x09}},"PARTITION",NULL,NULL,NULL},
 {{0x961578fe,0xb6b7,0x44c3,{0xaf,0x35,0x6b,0xc7,0x05,0xcd,0x2b,0x1f}},"FAT",NULL,NULL,NULL}
};
STATIC EFI_STATUS DriverCall(BOOLEAN Start,EFI_DRIVER_BINDING_PROTOCOL *This,EFI_HANDLE Handle,EFI_DEVICE_PATH_PROTOCOL *Remaining) {
 UINTN I;EFI_STATUS S;CHAR8 Text[96];
 for(I=0;I<ARRAY_SIZE(DiskDrivers) && DiskDrivers[I].Binding!=This;I++) {}
 if(I==ARRAY_SIZE(DiskDrivers)) return EFI_INVALID_PARAMETER;
 if(!TraceBindings) return Start?DiskDrivers[I].Start(This,Handle,Remaining):DiskDrivers[I].Supported(This,Handle,Remaining);
 AsciiSPrint(Text,sizeof(Text),"%a %a HANDLE %LX",DiskDrivers[I].Name,Start?"START":"SUPPORTED",(UINT64)(UINTN)Handle);
 BaytrailMarker(ContextH,17,Text,0xffffff);
 BaytrailMarker(ContextH,18,"DRIVER CALL PENDING",0xffffff);
 S=Start?DiskDrivers[I].Start(This,Handle,Remaining):DiskDrivers[I].Supported(This,Handle,Remaining);
 AsciiSPrint(Text,sizeof(Text),"%a %a RETURN %LX",DiskDrivers[I].Name,Start?"START":"SUPPORTED",(UINT64)S);
 BaytrailMarker(ContextH,18,Text,EFI_ERROR(S)?0xffa040:0x40ff40);return S;
}
STATIC EFI_STATUS EFIAPI DriverSupported(EFI_DRIVER_BINDING_PROTOCOL *This,EFI_HANDLE Handle,EFI_DEVICE_PATH_PROTOCOL *Remaining) {return DriverCall(FALSE,This,Handle,Remaining);}
STATIC EFI_STATUS EFIAPI DriverStart(EFI_DRIVER_BINDING_PROTOCOL *This,EFI_HANDLE Handle,EFI_DEVICE_PATH_PROTOCOL *Remaining) {return DriverCall(TRUE,This,Handle,Remaining);}
STATIC VOID TraceDiskDrivers(VOID) {
 EFI_HANDLE *Handles;UINTN Count,I,J;EFI_DRIVER_BINDING_PROTOCOL *Binding;EFI_LOADED_IMAGE_PROTOCOL *Image;EFI_DEVICE_PATH_PROTOCOL *Path;
 if(EFI_ERROR(gBS->LocateHandleBuffer(ByProtocol,&gEfiDriverBindingProtocolGuid,NULL,&Count,&Handles))) return;
 for(I=0;I<Count;I++) {
  if(EFI_ERROR(gBS->HandleProtocol(Handles[I],&gEfiDriverBindingProtocolGuid,(VOID **)&Binding)) ||
     EFI_ERROR(gBS->HandleProtocol(Binding->ImageHandle,&gEfiLoadedImageProtocolGuid,(VOID **)&Image))) continue;
  Path=Image->FilePath;if(!Path) continue;
  while(!IsDevicePathEnd(Path)) {
   if(DevicePathType(Path)==MEDIA_DEVICE_PATH && DevicePathSubType(Path)==MEDIA_PIWG_FW_FILE_DP) {
    MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *File=(VOID *)Path;
    for(J=0;J<ARRAY_SIZE(DiskDrivers);J++) if(!CompareMem(&File->FvFileName,&DiskDrivers[J].Guid,sizeof(EFI_GUID))) {
     DiskDrivers[J].Binding=Binding;DiskDrivers[J].Supported=Binding->Supported;DiskDrivers[J].Start=Binding->Start;
     Binding->Supported=DriverSupported;Binding->Start=DriverStart;break;
    }
   }
   Path=NextDevicePathNode(Path);
  }
 }
 FreePool(Handles);
 for(J=0;J<ARRAY_SIZE(DiskDrivers);J++) {
  Print(L"USB: disk driver trace %a %s\r\n",DiskDrivers[J].Name,DiskDrivers[J].Binding?L"READY":L"MISSING");
  Check(DiskDrivers[J].Binding?EFI_SUCCESS:EFI_NOT_FOUND,"USB DISK DRIVER TRACE MISSING");
 }
}
STATIC VOID ConnectProtocol(EFI_GUID *Guid) {
 EFI_HANDLE *Handles;UINTN Count,I;
 if(EFI_ERROR(gBS->LocateHandleBuffer(ByProtocol,Guid,NULL,&Count,&Handles))) return;
 for(I=0;I<Count;I++) {
  if(Guid==&gEfiBlockIoProtocolGuid) {
   EFI_BLOCK_IO_PROTOCOL *Io;
   if(!EFI_ERROR(gBS->HandleProtocol(Handles[I],Guid,(VOID **)&Io)))
    Print(L"USB: block handle %p media %u size %u last LBA %Lu partition %u\r\n",Handles[I],Io->Media->MediaId,Io->Media->BlockSize,Io->Media->LastBlock,Io->Media->LogicalPartition);
  }
  gBS->ConnectController(Handles[I],NULL,NULL,TRUE);
  if(Guid==&gEfiBlockIoProtocolGuid) Print(L"USB: block handle %p connect returned\r\n",Handles[I]);
 }
 FreePool(Handles);
}
STATIC VOID NativeUsb(EFI_HANDLE Image,EFI_SYSTEM_TABLE *St) {
 EFI_HANDLE *Handles,App;UINTN Count,I,Round,ExpectedSize=0,Size;UINT8 *Expected=NULL,*Bytes;
 EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Fs;EFI_FILE_PROTOCOL *Root,*File;EFI_DEVICE_PATH_PROTOCOL *Path;
 EFI_SIMPLE_TEXT_INPUT_PROTOCOL *Keyboard=NULL;EFI_STATUS S;EFI_USB2_HC_PROTOCOL *Hc;
 EFI_GUID ProbeGuid={0x4a7a8bbb,0xac8f,0x4f00,{0xaa,0xd1,0x8a,0x83,0xfa,0x91,0x10,0x74}};
 BOOLEAN Storage=FALSE;
 TraceDiskDrivers();
 Print(L"USB: connecting native X64 xHCI...\r\n");
 ConnectProtocol(&gEfiPciIoProtocolGuid);
 Check(gBS->LocateProtocol(&gEfiUsb2HcProtocolGuid,NULL,(VOID **)&Hc),"USB XHCI DRIVER FAILED");
 Print(L"USB: xHCI started; enumerating hubs/devices (15 s)...\r\n");
 for(Round=0;Round<150;Round++) {
  if(Round%10==0) Print(L"USB: enumerate tick %u/150 - timer wait...\r\n",(UINT32)Round);
  gBS->Stall(100000);
  if(Round%10==0) {
   Print(L"USB: connecting USB interfaces...\r\n");ConnectProtocol(&gEfiUsbIoProtocolGuid);
   Print(L"USB: connecting disks/FAT...\r\n");ConnectProtocol(&gEfiBlockIoProtocolGuid);
  }
 }
 Check(GetSectionFromAnyFv(&ProbeGuid,EFI_SECTION_RAW,0,(VOID **)&Expected,&ExpectedSize),"USB TEST REFERENCE MISSING");
 Check(gBS->LocateHandleBuffer(ByProtocol,&gEfiSimpleFileSystemProtocolGuid,NULL,&Count,&Handles),"USB FAT MISSING");
 for(I=0;I<Count;I++) {
  if(!HasUsbPath(Handles[I])) continue;
  if(EFI_ERROR(gBS->HandleProtocol(Handles[I],&gEfiSimpleFileSystemProtocolGuid,(VOID **)&Fs))) continue;
  if(EFI_ERROR(Fs->OpenVolume(Fs,&Root))) continue;
  S=Root->Open(Root,&File,L"\\EFI\\BOOT\\BOOTX64.EFI",EFI_FILE_MODE_READ,0);Root->Close(Root);
  if(EFI_ERROR(S)) continue;
  Bytes=AllocatePool(ExpectedSize+1);if(!Bytes) {File->Close(File);continue;}
  Size=ExpectedSize+1;S=File->Read(File,&Size,Bytes);File->Close(File);
  if(EFI_ERROR(S) || Size!=ExpectedSize || CompareMem(Bytes,Expected,Size)) {
   Print(L"USB: BOOTX64.EFI differs from the supplied test application; skipped.\r\n");FreePool(Bytes);continue;
  }
  FreePool(Bytes);Path=FileDevicePath(Handles[I],L"\\EFI\\BOOT\\BOOTX64.EFI");
  if(!Path) continue;
  Print(L"USB: verified test BOOTX64.EFI; LoadImage/StartImage...\r\n");
  S=gBS->LoadImage(FALSE,Image,Path,NULL,0,&App);FreePool(Path);Check(S,"USB LOAD IMAGE FAILED");
  Check(gBS->StartImage(App,NULL,NULL),"USB START IMAGE FAILED");
  gBS->UnloadImage(App);Storage=TRUE;break;
 }
 FreePool(Handles);FreePool(Expected);
 if(!Storage) Check(EFI_NOT_FOUND,"USB TEST BOOTX64 NOT FOUND");
 StorageOk=TRUE;BaytrailMarker(ContextH,21,"USB STORAGE X64 APP OK",0x40ff40);
 Check(gBS->LocateHandleBuffer(ByProtocol,&gEfiSimpleTextInProtocolGuid,NULL,&Count,&Handles),"USB KEYBOARD MISSING");
 for(I=0;I<Count;I++) {
  if(!HasUsbPath(Handles[I])) continue;
  if(!EFI_ERROR(gBS->HandleProtocol(Handles[I],&gEfiSimpleTextInProtocolGuid,(VOID **)&Keyboard))) {
   St->ConsoleInHandle=Handles[I];St->ConIn=Keyboard;break;
  }
 }
 FreePool(Handles);if(!Keyboard) Check(EFI_NOT_FOUND,"USB KEYBOARD MISSING");
 Print(L"USB: storage application returned; native keyboard connected.\r\n");
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
 NativeUsb(Image,St);
 {
  BAYTRAIL_USB_TRACE_CONTROL *Control;
  Check(gBS->LocateProtocol(&gBaytrailUsbTraceControlGuid,NULL,(VOID **)&Control),"USB TRACE CONTROL MISSING");
  if(Control->Revision==1) Control->Enabled=FALSE;TraceBindings=FALSE;
 }

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
 Out->ClearScreen(Out);Out->OutputString(Out,L"Baytrail: EDK2 X64 Shell / read-only RAM FAT\r\nNative USB storage test passed; USB keyboard is enabled.\r\n\r\n");
 Status=gBS->StartImage(App,NULL,NULL);
 Out->OutputString(Out,L"\r\nShell returned. Test stopped.\r\n");
 Check(Status,"SHELL START IMAGE FAILED");BaytrailHalt();return EFI_SUCCESS;
}
