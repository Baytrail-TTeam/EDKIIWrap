/* Read-only RAM FAT volume + genuine EDK2 LoadImage/StartImage.
 * No physical USB keyboard/storage/controller access in this bootstrap.
 */
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
STATIC EFI_GUID DiskGuid={0x9e4bcd6e,0x3c20,0x4d90,{0xa0,0xb8,0xf2,0x17,0x18,0x10,0x5c,0x93}};
STATIC UINT8 *Disk;STATIC UINTN DiskBytes;
STATIC EFI_BLOCK_IO_MEDIA Media={.MediaId=1,.MediaPresent=TRUE,.LogicalPartition=TRUE,
 .ReadOnly=TRUE,.BlockSize=512,.IoAlign=1,.LastBlock=8191};
STATIC EFI_BLOCK_IO_PROTOCOL Block;
STATIC EFI_SIMPLE_TEXT_INPUT_PROTOCOL Input;
STATIC EFI_HANDLE DiskHandle,InputHandle;
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
STATIC EFI_STATUS EFIAPI ResetInput(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This,BOOLEAN Extended) {(VOID)This;(VOID)Extended;return EFI_SUCCESS;}
STATIC EFI_STATUS EFIAPI ReadInput(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This,EFI_INPUT_KEY *Key) {(VOID)This;(VOID)Key;return EFI_NOT_READY;}
STATIC VOID EFIAPI WaitInput(EFI_EVENT Event,VOID *Context) {(VOID)Event;(VOID)Context;}
STATIC Handoff *ContextH;
STATIC BOOLEAN ScriptOk;
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
 return S;
}
STATIC VOID EFIAPI Idle(EFI_EVENT Event,VOID *Context) {
 (VOID)Event;(VOID)Context;
 BaytrailMarker(ContextH,20,ScriptOk?"SHELL X64 SCRIPT OK":"SHELL IDLE BEFORE SCRIPT",ScriptOk?0x40ff40:0xff4040);
 BaytrailHalt();
}
STATIC VOID Check(EFI_STATUS Status,CONST CHAR8 *Message) {
 if(EFI_ERROR(Status)) {BaytrailMarker(ContextH,7,Message,0xff4040);BaytrailHalt();}
}
EFI_STATUS EFIAPI ShellBootEntry(EFI_HANDLE Image,EFI_SYSTEM_TABLE *St) {
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
 Check(gBS->CreateEvent(EVT_NOTIFY_WAIT,TPL_CALLBACK,WaitInput,NULL,&Input.WaitForKey),"SHELL INPUT EVENT FAILED");
 Input.Reset=ResetInput;Input.ReadKeyStroke=ReadInput;
 Check(gBS->InstallProtocolInterface(&InputHandle,&gEfiSimpleTextInProtocolGuid,EFI_NATIVE_INTERFACE,&Input),"SHELL INPUT INSTALL FAILED");
 St->ConsoleInHandle=InputHandle;St->ConIn=&Input;
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
 /* Stop at the first idle wait. This bootstrap deliberately has no USB input. */
 Check(gBS->CreateEventEx(EVT_NOTIFY_SIGNAL,TPL_CALLBACK,Idle,NULL,&gIdleLoopEventGuid,&IdleEvent),"SHELL IDLE EVENT FAILED");
 OriginalOutput=Out->OutputString;Out->OutputString=ObserveOutput;
 Out->ClearScreen(Out);Out->OutputString(Out,L"Baytrail: EDK2 X64 Shell / read-only RAM FAT\r\nNative USB and keyboard are not enabled in this test.\r\n\r\n");
 Status=gBS->StartImage(App,NULL,NULL);
 Out->OutputString(Out,L"\r\nShell returned. Test stopped.\r\n");
 Check(Status,"SHELL START IMAGE FAILED");BaytrailHalt();return EFI_SUCCESS;
}
