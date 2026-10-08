#include <PiDxe.h>
#include <Pi/PiHob.h>
#include <Library/BaseLib.h>
#include <Library/HobLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include "M3Handoff.h"
#include "Marker.h"
STATIC BOOLEAN Asserting;
STATIC CONST Handoff *Context(VOID) {
 UINT8 *P=GetHobList();UINTN Seen=0,I;
 CONST UINT8 Guid[16]={0xa3,0xc6,0xd1,0x64,0xaf,0x58,0xf1,0x46,0x8b,0x58,0xa4,0x50,0x75,0xa0,0x03,0x13};
 if(!P) return NULL;
 while(Seen<65536) {
  EFI_HOB_GENERIC_HEADER *Hdr=(VOID *)P;
  if(Hdr->HobType==EFI_HOB_TYPE_END_OF_HOB_LIST || Hdr->HobLength<8 || (Hdr->HobLength&7)) return NULL;
  if(Hdr->HobType==EFI_HOB_TYPE_GUID_EXTENSION && Hdr->HobLength>=sizeof(EFI_HOB_GUID_TYPE)+sizeof(M3Handoff)) {
   EFI_HOB_GUID_TYPE *G=(VOID *)P;CONST UINT8 *B=(VOID *)&G->Name;
   for(I=0;I<16 && B[I]==Guid[I];I++) {}
   if(I==16) return (CONST Handoff *)(G+1);
  }
  Seen+=Hdr->HobLength;P+=Hdr->HobLength;
 }
 return NULL;
}
STATIC VOID Upper(CHAR8 *B) {while(*B) {if(*B>='a'&&*B<='z') *B-=32;B++;}}
VOID EFIAPI DebugVPrint(UINTN Level,CONST CHAR8 *Format,VA_LIST Args) {
 CONST Handoff *H;CHAR8 Text[128];
 if(!(Level&DEBUG_ERROR)) return;
 H=Context();if(!H) return;
 AsciiVSPrint(Text,sizeof(Text),Format,Args);Upper(Text);BaytrailMarker(H,7,Text,0xffa040);
}
VOID EFIAPI DebugPrint(UINTN Level,CONST CHAR8 *Format,...) {
 VA_LIST Args;VA_START(Args,Format);DebugVPrint(Level,Format,Args);VA_END(Args);
}
VOID EFIAPI DebugBPrint(UINTN Level,CONST CHAR8 *Format,BASE_LIST Args) {
 CONST Handoff *H;CHAR8 Text[128];if(!(Level&DEBUG_ERROR)) return;
 H=Context();if(!H) return;
 AsciiBSPrint(Text,sizeof(Text),Format,Args);Upper(Text);BaytrailMarker(H,7,Text,0xffa040);
}
VOID EFIAPI DebugAssert(CONST CHAR8 *File,UINTN Line,CONST CHAR8 *Description) {
 CONST Handoff *H;CONST CHAR8 *Base=File;CHAR8 Text[128];
 if(Asserting) BaytrailHalt();Asserting=TRUE;H=Context();
 if(H) {
  if(File) for(CONST CHAR8 *P=File;*P;P++) if(*P=='/'||*P=='\\') Base=P+1;
  BaytrailMarker(H,7,"M3 EDK2 ASSERT",0xff4040);
  AsciiSPrint(Text,sizeof(Text),"%a LINE %u",Base?Base:"UNKNOWN",(UINT32)Line);Upper(Text);
  BaytrailMarker(H,8,Text,0xff4040);
  AsciiSPrint(Text,sizeof(Text),"%a",Description?Description:"UNKNOWN");Upper(Text);
  BaytrailMarker(H,9,Text,0xff4040);
 }
 BaytrailHalt();
}
VOID *EFIAPI DebugClearMemory(VOID *Buffer,UINTN Length) {(VOID)Length;return Buffer;}
BOOLEAN EFIAPI DebugAssertEnabled(VOID) {return TRUE;}
BOOLEAN EFIAPI DebugPrintEnabled(VOID) {return TRUE;}
BOOLEAN EFIAPI DebugCodeEnabled(VOID) {return TRUE;}
BOOLEAN EFIAPI DebugClearMemoryEnabled(VOID) {return FALSE;}
BOOLEAN EFIAPI DebugPrintLevelEnabled(UINTN Level) {return (BOOLEAN)((Level&DEBUG_ERROR)!=0);}
