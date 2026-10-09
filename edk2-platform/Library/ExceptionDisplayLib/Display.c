/* Route DXE CPU exception diagnostics to the existing framebuffer.
 * No UART access, allocation, console protocol, USB or Boot Services calls. */
#include <PiDxe.h>
#include <Library/SerialPortLib.h>
#include <Library/HobLib.h>
#include <Library/BaseLib.h>
#include "M3Handoff.h"
#include "Marker.h"
STATIC CONST Handoff *Context;
STATIC CHAR8 Line[512];STATIC UINTN Length;STATIC UINT32 Row;
RETURN_STATUS EFIAPI SerialPortInitialize(VOID) {
 VOID *Hob=GetFirstGuidHob(&gBaytrailHandoffGuid);
 Context=NULL;Length=0;Row=7;
 if(Hob && GET_GUID_HOB_DATA_SIZE(Hob)==sizeof(M3Handoff)) Context=&((M3Handoff *)GET_GUID_HOB_DATA(Hob))->common;
 return Context?RETURN_SUCCESS:RETURN_NOT_FOUND;
}
STATIC VOID Field(CHAR8 *Text) {
 CHAR8 Short[48];UINTN N=0;
 if(!Text || Row>23) return;
 while(Text[N] && Text[N]!=',' && Text[N]!=')' && N+1<sizeof(Short)) {
  Short[N]=Text[N];if(Short[N]>='a' && Short[N]<='z') Short[N]-=32;N++;
 }
 Short[N]=0;BaytrailMarker(Context,Row++,Short,0xff4040);
}
STATIC VOID Emit(VOID) {
 Line[Length]=0;
 if(Context && Row<=23 &&
    (AsciiStrStr(Line,"Exception Type") || AsciiStrStr(Line,"ExceptionData") ||
     AsciiStrStr(Line,"RIP  -") || AsciiStrStr(Line,"RAX  -") ||
     AsciiStrStr(Line,"RBX  -") || AsciiStrStr(Line,"R8   -") ||
     AsciiStrStr(Line,"R11  -") || AsciiStrStr(Line,"R14  -") ||
     AsciiStrStr(Line,"CR0  -") || AsciiStrStr(Line,"CR4  -") ||
     AsciiStrStr(Line,"ImageBase="))) {
  if(AsciiStrStr(Line,"CR0  -")) {
   Field(AsciiStrStr(Line,"CR0"));Field(AsciiStrStr(Line,"CR2"));Field(AsciiStrStr(Line,"CR3"));Length=0;return;
  }
  if(AsciiStrStr(Line,"ImageBase=")) {
   Field(AsciiStrStr(Line,"ImageBase="));Field(AsciiStrStr(Line,"EntryPoint="));Length=0;return;
  }
  for(CHAR8 *P=Line;*P;P++) if(*P>='a' && *P<='z') *P-=32;
  BaytrailMarker(Context,Row++,Line,0xff4040);
 }
 Length=0;
}
UINTN EFIAPI SerialPortWrite(UINT8 *Buffer,UINTN Count) {
 if(!Buffer) return 0;
 for(UINTN I=0;I<Count;I++) {
  if(Buffer[I]=='\n') Emit();
  else if(Buffer[I]!='\r' && Length+1<sizeof(Line)) Line[Length++]=(CHAR8)Buffer[I];
 }
 return Count;
}
UINTN EFIAPI SerialPortRead(UINT8 *Buffer,UINTN Count) {(VOID)Buffer;(VOID)Count;return 0;}
BOOLEAN EFIAPI SerialPortPoll(VOID) {return FALSE;}
RETURN_STATUS EFIAPI SerialPortSetControl(UINT32 Control) {(VOID)Control;return RETURN_UNSUPPORTED;}
RETURN_STATUS EFIAPI SerialPortGetControl(UINT32 *Control) {(VOID)Control;return RETURN_UNSUPPORTED;}
RETURN_STATUS EFIAPI SerialPortSetAttributes(UINT64 *Baud,UINT32 *Fifo,UINT32 *Timeout,EFI_PARITY_TYPE *Parity,UINT8 *Bits,EFI_STOP_BITS_TYPE *Stop) {
 (VOID)Baud;(VOID)Fifo;(VOID)Timeout;(VOID)Parity;(VOID)Bits;(VOID)Stop;return RETURN_UNSUPPORTED;
}
