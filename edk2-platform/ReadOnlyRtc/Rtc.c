/* Read the existing PC RTC without changing its time, mode or alarms.
 * Port selection follows EDK2 PcRtc.c; GetTime performs no allocations or
 * Boot Services calls and remains in runtime code. No SetTime is installed. */
#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/IoLib.h>
#include <Library/HobLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include "M3Handoff.h"
extern EFI_GUID gBaytrailHandoffGuid;
STATIC UINT8 CenturyAddress;
STATIC UINT8 ReadRtc(UINT8 Address) {
 IoWrite8(0x70,(UINT8)(Address|(IoRead8(0x70)&0x80)));
 return IoRead8(0x71);
}
STATIC BOOLEAN Snapshot(UINT8 *Data) {
 CONST UINT8 Registers[]={0,2,4,7,8,9,0x0b};UINTN I;
 if((ReadRtc(0x0a)&0x80) || !(ReadRtc(0x0d)&0x80)) return FALSE;
 for(I=0;I<ARRAY_SIZE(Registers);I++) Data[I]=ReadRtc(Registers[I]);
 Data[7]=CenturyAddress?ReadRtc(CenturyAddress):0;
 return !(ReadRtc(0x0a)&0x80) && !(Data[6]&0x80);
}
STATIC BOOLEAN Decode(UINT8 Raw,BOOLEAN Binary,UINT8 *Value) {
 if(Binary) {*Value=Raw;return TRUE;}
 if((Raw&15)>9 || (Raw>>4)>9) return FALSE;
 *Value=(UINT8)((Raw>>4)*10+(Raw&15));return TRUE;
}
STATIC EFI_STATUS EFIAPI ReadTime(EFI_TIME *Time,EFI_TIME_CAPABILITIES *Caps) {
 UINT8 A[8],B[8],V[6],Century,Days,Hour;UINTN Attempt,I;
 BOOLEAN Interrupts,Binary,Stable=FALSE,Pm;
 CONST UINT8 MonthDays[]={31,28,31,30,31,30,31,31,30,31,30,31};
 if(!Time) return EFI_INVALID_PARAMETER;
 Interrupts=SaveAndDisableInterrupts();
 /* Bounded wait: an invalid/stopped RTC must not hang USB or Windows. */
 for(Attempt=0;Attempt<100000;Attempt++) {
  if(Snapshot(A) && Snapshot(B) && CompareMem(A,B,sizeof(A))==0) {Stable=TRUE;break;}
  CpuPause();
 }
 SetInterruptState(Interrupts);
 if(!Stable) return EFI_DEVICE_ERROR;
 Binary=(A[6]&4)!=0;Pm=(A[2]&0x80)!=0;A[2]&=0x7f;
 for(I=0;I<6;I++) if(!Decode(A[I],Binary,&V[I])) return EFI_DEVICE_ERROR;
 Hour=V[2];
 if(!(A[6]&2)) {if(Hour<1 || Hour>12) return EFI_DEVICE_ERROR;Hour=(UINT8)(Hour%12+(Pm?12:0));}
 /* As in PC EDK2, absent FADT century uses the 2000..2099 window. */
 Century=20;
 if(CenturyAddress && (!Decode(A[7],Binary,&Century) || Century<19 || Century>99)) return EFI_DEVICE_ERROR;
 if(V[0]>59 || V[1]>59 || Hour>23 || V[4]<1 || V[4]>12 || V[5]>99) return EFI_DEVICE_ERROR;
 ZeroMem(Time,sizeof(*Time));Time->Year=(UINT16)(Century*100+V[5]);
 if(Time->Year<1900 || Time->Year>9999) return EFI_DEVICE_ERROR;
 Days=MonthDays[V[4]-1];
 if(V[4]==2 && Time->Year%4==0 && (Time->Year%100!=0 || Time->Year%400==0)) Days++;
 if(V[3]<1 || V[3]>Days) return EFI_DEVICE_ERROR;
 Time->Month=V[4];Time->Day=V[3];Time->Hour=Hour;Time->Minute=V[1];Time->Second=V[0];
 Time->TimeZone=EFI_UNSPECIFIED_TIMEZONE;
 if(Caps) {Caps->Resolution=1;Caps->Accuracy=50000000;Caps->SetsToZero=FALSE;}
 return EFI_SUCCESS;
}
EFI_STATUS EFIAPI RtcEntry(EFI_HANDLE Image,EFI_SYSTEM_TABLE *St) {
 VOID *Hob=GetFirstGuidHob(&gBaytrailHandoffGuid);M3Handoff *M;UINTN I;EFI_TIME Time;EFI_STATUS S;
 if(!Hob || GET_GUID_HOB_DATA_SIZE(Hob)!=sizeof(*M)) return EFI_NOT_FOUND;
 M=GET_GUID_HOB_DATA(Hob);
 for(I=0;I<M->common.table_count;I++) {
  TableRef *T=&M->common.tables[I];
  if(T->signature==SIGNATURE_32('F','A','C','P') && T->length>108) {
   UINT8 Address=((UINT8 *)(UINTN)T->address)[108];
   if(Address>0x0d && Address<0x80) CenturyAddress=Address;
   break;
  }
 }
 S=ReadTime(&Time,NULL);if(EFI_ERROR(S)) return S;
 St->RuntimeServices->GetTime=ReadTime;
 St->RuntimeServices->Hdr.CRC32=0;
 S=gBS->CalculateCrc32(St->RuntimeServices,St->RuntimeServices->Hdr.HeaderSize,&St->RuntimeServices->Hdr.CRC32);
 if(EFI_ERROR(S)) return S;
 return gBS->InstallProtocolInterface(&Image,&gEfiRealTimeClockArchProtocolGuid,EFI_NATIVE_INTERFACE,NULL);
}
