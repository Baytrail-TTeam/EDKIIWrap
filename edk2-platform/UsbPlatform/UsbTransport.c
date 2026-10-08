/* Observe mass-storage USB I/O without changing the transport or payload. */
#include <Uefi.h>
#include <Protocol/UsbIo.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include "Marker.h"
#include "UsbTrace.h"
typedef struct {
 EFI_USB_IO_PROTOCOL *Io;EFI_USB_IO_BULK_TRANSFER Bulk;EFI_USB_IO_CONTROL_TRANSFER Control;
 UINT8 Command;UINTN Commands;
} TRANSPORT;
STATIC TRANSPORT Records[64];STATIC UINTN Count;
STATIC VOID *Registration;
STATIC CONST Handoff *Screen;
STATIC BAYTRAIL_USB_TRACE_CONTROL *TraceControl;
STATIC CHAR8 Lines[6][96];STATIC UINT32 Colors[6];
STATIC VOID Mark(UINT32 Row,CONST CHAR8 *Text,UINT32 Color) {
 AsciiStrCpyS(Lines[Row-11],sizeof(Lines[0]),Text);Colors[Row-11]=Color;
 BaytrailMarker(Screen,Row,Text,Color);
}
VOID RefreshUsbTransportTrace(VOID) {
 UINTN I;for(I=0;I<ARRAY_SIZE(Lines);I++) if(Lines[I][0]) BaytrailMarker(Screen,(UINT32)I+11,Lines[I],Colors[I]);
}
STATIC TRANSPORT *Find(EFI_USB_IO_PROTOCOL *Io) {
 UINTN I;for(I=0;I<Count;I++) if(Records[I].Io==Io) return &Records[I];return NULL;
}
STATIC EFI_STATUS EFIAPI Bulk(EFI_USB_IO_PROTOCOL *Io,UINT8 Endpoint,VOID *Data,UINTN *Length,UINTN Timeout,UINT32 *Result) {
 TRANSPORT *T=Find(Io);EFI_STATUS S;CHAR8 Text[96];UINT8 *Bytes=Data;
 if(!T) return EFI_DEVICE_ERROR;
 if(!TraceControl->Enabled) return T->Bulk(Io,Endpoint,Data,Length,Timeout,Result);
 if(!(Endpoint&0x80) && Data && Length && *Length==31 && ReadUnaligned32(Data)==0x43425355) {
  T->Command=Bytes[15];T->Commands++;
  AsciiSPrint(Text,sizeof(Text),"USB CMD %u OP %02X TAG %X",(UINT32)T->Commands,T->Command,ReadUnaligned32((UINT32 *)(Bytes+4)));
  Mark(13,Text,0xffffff);
 }
 AsciiSPrint(Text,sizeof(Text),"USB BULK EP %02X BYTES %Lu MS %Lu",Endpoint,Length?(UINT64)*Length:0,(UINT64)Timeout);
 Mark(14,Text,0xffffff);Mark(15,"USB BULK PENDING",0xffffff);
 S=T->Bulk(Io,Endpoint,Data,Length,Timeout,Result);
 AsciiSPrint(Text,sizeof(Text),"BULK RETURN %LX USB %X LEN %Lu",(UINT64)S,Result?*Result:0,Length?(UINT64)*Length:0);
 Mark(15,Text,EFI_ERROR(S)?0xffa040:0x40ff40);
 if(!EFI_ERROR(S) && (Endpoint&0x80) && Data && Length) {
  if(*Length==13 && ReadUnaligned32(Data)==0x53425355) {
   AsciiSPrint(Text,sizeof(Text),"USB CSW STATUS %u TAG %X",Bytes[12],ReadUnaligned32((UINT32 *)(Bytes+4)));
   Mark(16,Text,Bytes[12]?0xffa040:0x40ff40);
  } else if(T->Command==3 && *Length>=14) {
   AsciiSPrint(Text,sizeof(Text),"USB SENSE KEY %X ASC %02X ASCQ %02X",Bytes[2]&15,Bytes[12],Bytes[13]);
   Mark(12,Text,0xffa040);
  }
 }
 return S;
}
STATIC EFI_STATUS EFIAPI Control(EFI_USB_IO_PROTOCOL *Io,EFI_USB_DEVICE_REQUEST *Request,EFI_USB_DATA_DIRECTION Direction,UINT32 Timeout,VOID *Data,UINTN Length,UINT32 *Result) {
 TRANSPORT *T=Find(Io);EFI_STATUS S;CHAR8 Text[96];
 if(!T) return EFI_DEVICE_ERROR;
 if(!TraceControl->Enabled) return T->Control(Io,Request,Direction,Timeout,Data,Length,Result);
 AsciiSPrint(Text,sizeof(Text),"USB CONTROL REQ %02X MS %u PENDING",Request?Request->Request:0,Timeout);
 Mark(11,Text,0xffffff);
 S=T->Control(Io,Request,Direction,Timeout,Data,Length,Result);
 AsciiSPrint(Text,sizeof(Text),"USB CONTROL RETURN %LX USB %X",(UINT64)S,Result?*Result:0);
 Mark(11,Text,EFI_ERROR(S)?0xffa040:0x40ff40);return S;
}
STATIC VOID EFIAPI Notify(EFI_EVENT Event,VOID *Context) {
 EFI_HANDLE Handle;UINTN Size;EFI_USB_IO_PROTOCOL *Io;EFI_USB_INTERFACE_DESCRIPTOR Interface;
 (VOID)Event;(VOID)Context;
 for(;;) {
  Size=sizeof(Handle);
  if(EFI_ERROR(gBS->LocateHandle(ByRegisterNotify,NULL,Registration,&Size,&Handle))) break;
  if(EFI_ERROR(gBS->HandleProtocol(Handle,&gEfiUsbIoProtocolGuid,(VOID **)&Io)) || Find(Io) || Count==ARRAY_SIZE(Records)) continue;
  if(EFI_ERROR(Io->UsbGetInterfaceDescriptor(Io,&Interface)) || Interface.InterfaceClass!=8) continue;
  Records[Count].Io=Io;Records[Count].Bulk=Io->UsbBulkTransfer;Records[Count].Control=Io->UsbControlTransfer;Count++;
  Io->UsbBulkTransfer=Bulk;Io->UsbControlTransfer=Control;
 }
}
EFI_STATUS TraceUsbTransport(CONST Handoff *H,BAYTRAIL_USB_TRACE_CONTROL *State) {
 EFI_EVENT Event;EFI_STATUS S;Screen=H;TraceControl=State;
 S=gBS->CreateEvent(EVT_NOTIFY_SIGNAL,TPL_NOTIFY,Notify,NULL,&Event);if(EFI_ERROR(S)) return S;
 return gBS->RegisterProtocolNotify(&gEfiUsbIoProtocolGuid,Event,&Registration);
}
