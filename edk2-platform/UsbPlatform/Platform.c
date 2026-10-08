/* Cooperative PM-timer ticks for this IRQ-disabled USB proof of concept.
 * Timers advance on Stall, idle wait and explicit pump. This is not a
 * general PI interrupt timer or an OS-ready architectural implementation. */
#include <PiDxe.h>
#include <Protocol/Timer.h>
#include <Protocol/Metronome.h>
#include <Protocol/BlockIo.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/TimerLib.h>
#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include <Library/HobLib.h>
#include <Guid/IdleLoopEvent.h>
#include "M3Handoff.h"
#include "Marker.h"
#include "UsbTrace.h"
STATIC BAYTRAIL_USB_TRACE_CONTROL TraceControl={1,TRUE};
extern EFI_GUID gBaytrailHandoffGuid;
EFI_STATUS PublishXhci(M3Handoff *M);
EFI_STATUS TraceUsbTransport(CONST Handoff *H,BAYTRAIL_USB_TRACE_CONTROL *State);
VOID RefreshUsbTransportTrace(VOID);
STATIC EFI_TIMER_NOTIFY Handler;
STATIC UINT64 Period=10000,Last,Mask;
STATIC BOOLEAN Busy;
STATIC CONST Handoff *DiagnosticH;
typedef struct {EFI_BLOCK_IO_PROTOCOL *Io;EFI_BLOCK_READ Read;UINTN Index;} READ_RECORD;
STATIC READ_RECORD Reads[64];STATIC UINTN ReadCount,ReadDepth,CurrentDisk;
STATIC EFI_LBA CurrentLba;
STATIC UINT64 ReadTicks,ReadElapsed,ReadReportAt;
STATIC VOID ReadWatchdog(VOID) {
 UINT64 Now;CHAR8 Text[96];
 if(!ReadDepth) return;
 Now=GetPerformanceCounter();ReadElapsed+=(Now-ReadTicks)&Mask;ReadTicks=Now;
 if(ReadElapsed>=ReadReportAt && ReadElapsed<3579545ULL*120) {
  RefreshUsbTransportTrace();
  AsciiSPrint(Text,sizeof(Text),"USB DISK READ WAIT %u S",(UINT32)(ReadElapsed/3579545));
  BaytrailMarker(DiagnosticH,17,Text,0xffa040);ReadReportAt+=3579545ULL*30;
 }
 if(ReadElapsed>=3579545ULL*120) {
  RefreshUsbTransportTrace();
  BaytrailMarker(DiagnosticH,17,"USB DISK READ TIMEOUT",0xff4040);
  AsciiSPrint(Text,sizeof(Text),"DISK %u LBA %Lu LAST %Lu",(UINT32)CurrentDisk,CurrentLba,Reads[CurrentDisk].Io->Media->LastBlock);
  BaytrailMarker(DiagnosticH,18,Text,0xff4040);BaytrailHalt();
 }
}
STATIC VOID *BlockRegistration;
STATIC EFI_TIMER_ARCH_PROTOCOL Timer;
STATIC EFI_STATUS EFIAPI Pump(EFI_TIMER_ARCH_PROTOCOL *This) {
 UINT64 Now,Delta;EFI_TPL Old;(VOID)This;
 ReadWatchdog();
 if(Busy || !Handler || !Period) return EFI_SUCCESS;
 Busy=TRUE;Now=GetPerformanceCounter();Delta=(Now-Last)&Mask;
 if(Delta*10000000ULL>=Period*3579545ULL) {
  Last=Now;Old=gBS->RaiseTPL(TPL_HIGH_LEVEL);
  Handler(Delta*10000000ULL/3579545);
  /* RestoreTPL dispatches USB callbacks. Those callbacks must be able to
   * advance timers while polling media readiness or waiting for transfers. */
  Busy=FALSE;gBS->RestoreTPL(Old);return EFI_SUCCESS;
 }
 Busy=FALSE;return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI Stall(UINTN Us);
STATIC EFI_CHECK_EVENT OriginalCheckEvent;
STATIC EFI_STATUS EFIAPI CheckEvent(EFI_EVENT Event) {
 /* Media NOT_READY retries can poll CheckEvent without Stall. */
 Pump(&Timer);return OriginalCheckEvent(Event);
}
STATIC BOOLEAN TimerTestDone,TimerTestOk;
STATIC VOID EFIAPI TimerTest(EFI_EVENT Event,VOID *Context) {
 EFI_EVENT Timeout;UINT64 Start;EFI_STATUS S;(VOID)Event;(VOID)Context;
 S=gBS->CreateEvent(EVT_TIMER,TPL_CALLBACK,NULL,NULL,&Timeout);
 if(EFI_ERROR(S)) {TimerTestDone=TRUE;return;}
 S=gBS->SetTimer(Timeout,TimerRelative,100000);Start=GetPerformanceCounter();
 while(!EFI_ERROR(S) && EFI_ERROR(gBS->CheckEvent(Timeout))) {
  if(((GetPerformanceCounter()-Start)&Mask)>3579545/5) {S=EFI_TIMEOUT;break;}
 }
 TimerTestOk=!EFI_ERROR(S);TimerTestDone=TRUE;gBS->CloseEvent(Timeout);
}
STATIC EFI_STATUS TestCallbackTimeout(VOID) {
 EFI_EVENT Event;UINT64 Start;EFI_STATUS S;
 S=gBS->CreateEvent(EVT_TIMER|EVT_NOTIFY_SIGNAL,TPL_CALLBACK,TimerTest,NULL,&Event);
 if(EFI_ERROR(S)) return S;
 S=gBS->SetTimer(Event,TimerRelative,10000);Start=GetPerformanceCounter();
 while(!EFI_ERROR(S) && !TimerTestDone) {
  Stall(1000);if(((GetPerformanceCounter()-Start)&Mask)>3579545/2) {S=EFI_TIMEOUT;break;}
 }
 gBS->CloseEvent(Event);return !EFI_ERROR(S) && TimerTestOk?EFI_SUCCESS:EFI_DEVICE_ERROR;
}
STATIC EFI_STATUS EFIAPI Register(EFI_TIMER_ARCH_PROTOCOL *This,EFI_TIMER_NOTIFY Notify) {
 (VOID)This;if(Notify && Handler) return EFI_ALREADY_STARTED;
 if(!Notify && !Handler) return EFI_INVALID_PARAMETER;Handler=Notify;Last=GetPerformanceCounter();return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI SetPeriod(EFI_TIMER_ARCH_PROTOCOL *This,UINT64 New) {
 (VOID)This;if(New>10000000) return EFI_UNSUPPORTED;Period=New;Last=GetPerformanceCounter();return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI GetPeriod(EFI_TIMER_ARCH_PROTOCOL *This,UINT64 *Value) {
 (VOID)This;if(!Value) return EFI_INVALID_PARAMETER;*Value=Period;return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI Stall(UINTN Us) {
 while(Us) {UINTN Part=MIN(Us,1000);MicroSecondDelay(Part);Us-=Part;Pump(&Timer);}return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI WaitForTick(EFI_METRONOME_ARCH_PROTOCOL *This,UINT32 Ticks) {
 (VOID)This;return Stall((UINTN)Ticks*10);
}
STATIC EFI_METRONOME_ARCH_PROTOCOL Metronome={WaitForTick,100};
STATIC VOID EFIAPI Idle(EFI_EVENT Event,VOID *Context) {(VOID)Event;(VOID)Context;Stall(1000);}
/* Deny writes without changing driver-owned media state. Removable USB
 * devices refresh ReadOnly from MODE SENSE; forcing it here causes a
 * false media change and repeated protocol reinstall on every read. */
STATIC EFI_STATUS EFIAPI DenyWrite(EFI_BLOCK_IO_PROTOCOL *This,UINT32 Id,EFI_LBA Lba,UINTN Size,VOID *Buffer) {
 (VOID)This;(VOID)Id;(VOID)Lba;(VOID)Size;(VOID)Buffer;return EFI_WRITE_PROTECTED;
}
STATIC EFI_STATUS EFIAPI TraceRead(EFI_BLOCK_IO_PROTOCOL *Io,UINT32 Id,EFI_LBA Lba,UINTN Size,VOID *Buffer) {
 UINTN I,SavedDisk=CurrentDisk;EFI_LBA SavedLba=CurrentLba;EFI_STATUS S;CHAR8 Text[96];
 for(I=0;I<ReadCount && Reads[I].Io!=Io;I++) {}
 if(I==ReadCount) return EFI_DEVICE_ERROR;
 if(!ReadDepth) {ReadElapsed=0;ReadReportAt=3579545ULL*30;ReadTicks=GetPerformanceCounter();}
 ReadDepth++;CurrentDisk=Reads[I].Index;CurrentLba=Lba;
 AsciiSPrint(Text,sizeof(Text),"DISK %u READ LBA %Lu BYTES %Lu",(UINT32)CurrentDisk,Lba,(UINT64)Size);
 if(TraceControl.Enabled) {BaytrailMarker(DiagnosticH,19,Text,0xffffff);BaytrailMarker(DiagnosticH,20,"DISK READ PENDING",0xffffff);}
 S=Reads[I].Read(Io,Id,Lba,Size,Buffer);
 AsciiSPrint(Text,sizeof(Text),"DISK %u READ RETURN %LX",(UINT32)CurrentDisk,(UINT64)S);
 if(TraceControl.Enabled) BaytrailMarker(DiagnosticH,20,Text,EFI_ERROR(S)?0xffa040:0x40ff40);
 ReadDepth--;CurrentDisk=SavedDisk;CurrentLba=SavedLba;return S;
}
STATIC VOID EFIAPI GuardBlocks(EFI_EVENT Event,VOID *Context) {
 EFI_HANDLE Handle;UINTN Size,I;EFI_BLOCK_IO_PROTOCOL *Io;(VOID)Event;(VOID)Context;
 for(;;) {
  Size=sizeof(Handle);
  if(EFI_ERROR(gBS->LocateHandle(ByRegisterNotify,NULL,BlockRegistration,&Size,&Handle))) break;
  if(!EFI_ERROR(gBS->HandleProtocol(Handle,&gEfiBlockIoProtocolGuid,(VOID **)&Io))) {
   Io->WriteBlocks=DenyWrite;
   for(I=0;I<ReadCount && Reads[I].Io!=Io;I++) {}
   if(I==ReadCount && ReadCount<ARRAY_SIZE(Reads)) {
    Reads[I].Io=Io;Reads[I].Read=Io->ReadBlocks;Reads[I].Index=I;ReadCount++;
    Io->ReadBlocks=TraceRead;
   }
  }
 }
}
EFI_STATUS EFIAPI UsbPlatformEntry(EFI_HANDLE Image,EFI_SYSTEM_TABLE *St) {
 VOID *Hob=GetFirstGuidHob(&gBaytrailHandoffGuid);M3Handoff *M;EFI_HANDLE Handle=NULL;
 EFI_EVENT Event;EFI_STATUS S;UINT32 Crc;(VOID)Image;(VOID)St;
 if(!Hob || GET_GUID_HOB_DATA_SIZE(Hob)!=sizeof(*M)) return EFI_NOT_FOUND;
 M=GET_GUID_HOB_DATA(Hob);DiagnosticH=&M->common;
 if(GetPerformanceCounterProperties(NULL,&Mask)!=3579545) {
  BaytrailMarker(&M->common,8,"USB PM TIMER UNAVAILABLE",0xff4040);return EFI_DEVICE_ERROR;
 }
 Timer.RegisterHandler=Register;Timer.SetTimerPeriod=SetPeriod;Timer.GetTimerPeriod=GetPeriod;Timer.GenerateSoftInterrupt=Pump;
 Last=GetPerformanceCounter();S=gBS->InstallMultipleProtocolInterfaces(&Handle,&gEfiTimerArchProtocolGuid,&Timer,&gEfiMetronomeArchProtocolGuid,&Metronome,&gBaytrailUsbTraceControlGuid,&TraceControl,NULL);
 if(EFI_ERROR(S)) return S;
 OriginalCheckEvent=gBS->CheckEvent;gBS->CheckEvent=CheckEvent;
 gBS->Hdr.CRC32=0;gBS->CalculateCrc32(gBS,gBS->Hdr.HeaderSize,&Crc);gBS->Hdr.CRC32=Crc;
 S=TestCallbackTimeout();
 BaytrailMarker(&M->common,9,EFI_ERROR(S)?"USB TIMER CALLBACK FAILED":"USB TIMER CALLBACK OK",EFI_ERROR(S)?0xff4040:0x40ff40);
 if(EFI_ERROR(S)) return S;
 S=gBS->CreateEventEx(EVT_NOTIFY_SIGNAL,TPL_CALLBACK,Idle,NULL,&gIdleLoopEventGuid,&Event);
 if(EFI_ERROR(S)) return S;
 S=gBS->CreateEvent(EVT_NOTIFY_SIGNAL,TPL_NOTIFY,GuardBlocks,NULL,&Event);
 if(EFI_ERROR(S)) return S;
 S=gBS->RegisterProtocolNotify(&gEfiBlockIoProtocolGuid,Event,&BlockRegistration);
 if(EFI_ERROR(S)) return S;
 S=TraceUsbTransport(&M->common,&TraceControl);if(EFI_ERROR(S)) return S;
 S=PublishXhci(M);BaytrailMarker(&M->common,8,EFI_ERROR(S)?"USB PCI PUBLISH FAILED":"USB PCI AND PM TIMER READY",EFI_ERROR(S)?0xff4040:0x40ff40);
 return S;
}
