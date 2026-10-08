/* Discover the already enabled ACPI PM timer from the validated FADT.
 * No LPC/ACPI configuration writes, no TSC frequency assumptions. */
#include <PiDxe.h>
#include <Library/TimerLib.h>
#include <Library/IoLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/HobLib.h>
#include "M3Handoff.h"
extern EFI_GUID gBaytrailHandoffGuid;
STATIC UINT16 Port;
STATIC UINT32 Mask;
EFI_STATUS EFIAPI PmTimerConstructor(EFI_HANDLE Image,EFI_SYSTEM_TABLE *St) {
 VOID *Hob=GetFirstGuidHob(&gBaytrailHandoffGuid);M3Handoff *M;UINTN I;UINT8 *F;
 UINT32 P,Flags;UINT64 Extended;
 (VOID)Image;(VOID)St;
 if(!Hob || GET_GUID_HOB_DATA_SIZE(Hob)!=sizeof(*M)) return EFI_NOT_FOUND;
 M=GET_GUID_HOB_DATA(Hob);
 for(I=0;I<M->common.table_count;I++) {
  TableRef *T=&M->common.tables[I];
  if(T->signature!=SIGNATURE_32('F','A','C','P') || T->length<116) continue;
  F=(VOID *)(UINTN)T->address;CopyMem(&P,F+76,4);CopyMem(&Flags,F+112,4);
  if(T->length>=220 && F[208]==1 && F[209]>=24 && F[210]==0) {
   CopyMem(&Extended,F+212,8);if(Extended && Extended<=0xfffc) P=(UINT32)Extended;
  }
  if(!P || P>0xfffc || F[91]<4) continue;
  Port=(UINT16)P;Mask=(Flags&BIT8)?MAX_UINT32:0xffffff;
  /* Require a changing counter before controller takeover. */
  P=IoRead32(Port)&Mask;
  for(I=0;I<100000;I++) if((IoRead32(Port)&Mask)!=P) return EFI_SUCCESS;
  Port=0;return EFI_DEVICE_ERROR;
 }
 return EFI_NOT_FOUND;
}
UINT64 EFIAPI GetPerformanceCounter(VOID) {return Port?(IoRead32(Port)&Mask):0;}
UINT64 EFIAPI GetPerformanceCounterProperties(UINT64 *Start,UINT64 *End) {
 if(Start) *Start=0;if(End) *End=Mask;return Port?3579545:0;
}
UINT64 EFIAPI GetTimeInNanoSecond(UINT64 Ticks) {
 return (Ticks/3579545)*1000000000ULL+(Ticks%3579545)*1000000000ULL/3579545;
}
UINTN EFIAPI MicroSecondDelay(UINTN Us) {
 UINTN Original=Us;UINT32 Last,Now;UINT64 Left,Step;
 if(!Port) return 0;
 while(Us) {
  Step=MIN(Us,1000000);Us-=(UINTN)Step;Left=(Step*3579545+999999)/1000000;
  Last=(UINT32)GetPerformanceCounter();
  while(Left) {
   Now=(UINT32)GetPerformanceCounter();Step=(Now-Last)&Mask;
   if(Step>=Left) break;Left-=Step;Last=Now;CpuPause();
  }
 }
 return Original;
}
UINTN EFIAPI NanoSecondDelay(UINTN Ns) {MicroSecondDelay(Ns/1000+(Ns%1000!=0));return Ns;}
