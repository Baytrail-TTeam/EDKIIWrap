/* Reuse M3 checks; complete this milestone only after GOP calls pass. */
#define BAYTRAIL_PROBE_CONTINUE
#define ProbeEntry M3Checks
#include "../DxeProbe/Probe.c"
#undef ProbeEntry
#include <Protocol/GraphicsOutput.h>
#include <Protocol/DevicePath.h>
#include <Library/MemoryAllocationLib.h>
STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL *TestGop;
STATIC Handoff *TestH;
STATIC VOID Check(BOOLEAN Good,CONST CHAR8 *Message) {
 if(!Good) {BaytrailMarker(TestH,7,Message,0xff4040);BaytrailHalt();}
}
STATIC VOID Good(EFI_STATUS Status,CONST CHAR8 *Message) {Check(!EFI_ERROR(Status),Message);}
STATIC EFI_GRAPHICS_OUTPUT_BLT_PIXEL Color(UINT8 R,UINT8 G,UINT8 B) {
 EFI_GRAPHICS_OUTPUT_BLT_PIXEL P={B,G,R,0};return P;
}
STATIC BOOLEAN Same(EFI_GRAPHICS_OUTPUT_BLT_PIXEL A,EFI_GRAPHICS_OUTPUT_BLT_PIXEL B) {
 return A.Red==B.Red && A.Green==B.Green && A.Blue==B.Blue;
}
STATIC VOID DrawText(UINT32 Row,CONST CHAR8 *Text) {
 Handoff Canvas=*TestH;EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Buffer=AllocateZeroPool(900*36*4);
 Check(Buffer!=NULL,"M4 TEXT ALLOCATION FAILED");
 Canvas.framebuffer=(UINTN)Buffer;Canvas.framebuffer_size=900*36*4;
 Canvas.width=900;Canvas.height=36;Canvas.stride=900;Canvas.pixel_format=1;
 BaytrailMarker(&Canvas,0,Text,0x40ff40);
 Good(TestGop->Blt(TestGop,Buffer,EfiBltBufferToVideo,0,0,0,Row*32,MIN(TestH->width,900),36,900*4),"M4 TEXT BLT FAILED");
 FreePool(Buffer);
}
EFI_STATUS EFIAPI M4ProbeEntry(EFI_HANDLE Image,EFI_SYSTEM_TABLE *St) {
 VOID *Mh=GetFirstGuidHob(&gBaytrailHandoffGuid),*DevicePath;
 EFI_STATUS Status;EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Copy=NULL;UINTN Size=0,I,X,Y;
 EFI_GRAPHICS_OUTPUT_BLT_PIXEL Source[10*10],Out[10*10],Pixel,Expected;
 EFI_HANDLE *Handles;UINTN Count;UINT32 Padding=0;volatile UINT32 *PaddingAddress=NULL;
 UINT64 Span;M3Handoff *M;
 if(!Mh || GET_GUID_HOB_DATA_SIZE(Mh)!=sizeof(*M)) BaytrailHalt();
 M=GET_GUID_HOB_DATA(Mh);TestH=&M->common;
 Check(TestH->width>=320 && TestH->height>=352,"M4 DISPLAY TOO SMALL");
 Good(M3Checks(Image,St),"M4 M3 CHECKS FAILED");
 Status=gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid,NULL,(VOID **)&TestGop);
 Good(Status,"M4 GOP NOT FOUND");Check(TestGop && TestGop->Mode && TestGop->Mode->Info,"M4 GOP MODE MISSING");
 Good(gBS->LocateHandleBuffer(ByProtocol,&gEfiGraphicsOutputProtocolGuid,NULL,&Count,&Handles),"M4 GOP HANDLE FAILED");
 Check(Count==1,"M4 GOP HANDLE COUNT FAILED");
 Good(gBS->HandleProtocol(Handles[0],&gEfiDevicePathProtocolGuid,&DevicePath),"M4 DEVICE PATH FAILED");
 Check(DevicePath!=NULL,"M4 DEVICE PATH MISSING");FreePool(Handles);
 Check(TestGop->Mode->MaxMode==1 && TestGop->Mode->Mode==0 &&
  TestGop->Mode->FrameBufferBase==TestH->framebuffer && TestGop->Mode->FrameBufferSize==TestH->framebuffer_size,"M4 GOP FRAMEBUFFER FAILED");
 Good(TestGop->QueryMode(TestGop,0,&Size,&Copy),"M4 QUERY MODE FAILED");
 Check(Size==sizeof(*Copy) && Copy!=TestGop->Mode->Info && Copy->Version==0 &&
  Copy->HorizontalResolution==TestH->width && Copy->VerticalResolution==TestH->height &&
  Copy->PixelsPerScanLine==TestH->stride && Copy->PixelFormat==TestH->pixel_format &&
  !CompareMem(&Copy->PixelInformation,&TestGop->Mode->Info->PixelInformation,sizeof(Copy->PixelInformation)),"M4 MODE INFO FAILED");
 Copy->HorizontalResolution=1;Check(TestGop->Mode->Info->HorizontalResolution==TestH->width,"M4 QUERY OWNERSHIP FAILED");FreePool(Copy);
 Check(TestGop->QueryMode(TestGop,1,&Size,&Copy)==EFI_INVALID_PARAMETER &&
  TestGop->QueryMode(TestGop,0,NULL,&Copy)==EFI_INVALID_PARAMETER &&
  TestGop->QueryMode(TestGop,0,&Size,NULL)==EFI_INVALID_PARAMETER &&
  TestGop->SetMode(TestGop,1)==EFI_UNSUPPORTED,"M4 INVALID MODE FAILED");
 Span=(UINT64)TestH->stride*TestH->height*4;
 if(TestH->stride>TestH->width && Span<=TestH->framebuffer_size) {
  PaddingAddress=(VOID *)(UINTN)(TestH->framebuffer+((UINT64)(TestH->height-1)*TestH->stride+TestH->width)*4);
  Padding=*PaddingAddress;
 }
 Good(TestGop->SetMode(TestGop,0),"M4 SET MODE FAILED");
 if(PaddingAddress) Check(*PaddingAddress==Padding,"M4 STRIDE PADDING CHANGED");
 /* SetMode clears the visible area. Check all pixels, allowing reserved bits. */
 for(Y=0;Y<TestH->height;Y++) {
  volatile UINT32 *Row=(VOID *)(UINTN)(TestH->framebuffer+(UINT64)Y*TestH->stride*4);
  UINT32 Mask=TestH->pixel_format==PixelBitMask?TestH->red_mask|TestH->green_mask|TestH->blue_mask:0xffffff;
  for(X=0;X<TestH->width;X++) Check((Row[X]&Mask)==0,"M4 MODE CLEAR FAILED");
 }
 Pixel=Color(32,64,128);
 Good(TestGop->Blt(TestGop,&Pixel,EfiBltVideoFill,0,0,40,340,12,12,0),"M4 FILL FAILED");
 Good(TestGop->Blt(TestGop,Out,EfiBltVideoToBltBuffer,40,340,0,0,8,8,0),"M4 READBACK FAILED");
 for(I=0;I<64;I++) Check(Same(Out[I],Pixel),"M4 FILL COLOR FAILED");
 /* Buffer subrectangle, nonzero offsets and padded Delta. */
 for(Y=0;Y<10;Y++) for(X=0;X<10;X++) Source[Y*10+X]=Color((UINT8)(16+X*16),(UINT8)(32+Y*16),64);
 Good(TestGop->Blt(TestGop,Source,EfiBltBufferToVideo,1,2,100,340,8,8,40),"M4 BUFFER WRITE FAILED");
 SetMem(Out,sizeof(Out),0xa5);
 Good(TestGop->Blt(TestGop,Out,EfiBltVideoToBltBuffer,100,340,1,1,8,8,40),"M4 BUFFER READ FAILED");
 for(Y=0;Y<8;Y++) for(X=0;X<8;X++) Check(Same(Out[(Y+1)*10+X+1],Source[(Y+2)*10+X+1]),"M4 BUFFER COLORS FAILED");
 Check(Out[0].Red==0xa5 && Out[99].Blue==0xa5,"M4 BUFFER GUARD FAILED");
 /* Descending overlapping copy must use last actual row and preserve next row. */
 Good(TestGop->Blt(TestGop,Source,EfiBltBufferToVideo,0,0,200,340,10,10,0),"M4 COPY SEED FAILED");
 Good(TestGop->Blt(TestGop,NULL,EfiBltVideoToVideo,200,340,201,341,8,8,0),"M4 OVERLAP DOWN FAILED");
 Good(TestGop->Blt(TestGop,Out,EfiBltVideoToBltBuffer,200,340,0,0,10,10,0),"M4 OVERLAP READ FAILED");
 for(Y=0;Y<10;Y++) for(X=0;X<10;X++) {
  Expected=(X>=1&&X<=8&&Y>=1&&Y<=8)?Source[(Y-1)*10+X-1]:Source[Y*10+X];
  Check(Same(Out[Y*10+X],Expected),"M4 OVERLAP DOWN DATA FAILED");
 }
 Good(TestGop->Blt(TestGop,NULL,EfiBltVideoToVideo,201,341,200,340,8,8,0),"M4 OVERLAP UP FAILED");
 Good(TestGop->Blt(TestGop,Out,EfiBltVideoToBltBuffer,200,340,0,0,8,8,0),"M4 OVERLAP UP READ FAILED");
 for(Y=0;Y<8;Y++) for(X=0;X<8;X++) Check(Same(Out[Y*8+X],Source[Y*10+X]),"M4 OVERLAP UP DATA FAILED");
 Good(TestGop->Blt(TestGop,Source,EfiBltBufferToVideo,0,0,200,340,10,10,0),"M4 HORIZONTAL SEED FAILED");
 Good(TestGop->Blt(TestGop,NULL,EfiBltVideoToVideo,200,340,201,340,8,8,0),"M4 OVERLAP RIGHT FAILED");
 Good(TestGop->Blt(TestGop,Out,EfiBltVideoToBltBuffer,201,340,0,0,8,8,0),"M4 OVERLAP RIGHT READ FAILED");
 for(Y=0;Y<8;Y++) for(X=0;X<8;X++) Check(Same(Out[Y*8+X],Source[Y*10+X]),"M4 OVERLAP RIGHT DATA FAILED");
 Good(TestGop->Blt(TestGop,NULL,EfiBltVideoToVideo,201,340,200,340,8,8,0),"M4 OVERLAP LEFT FAILED");
 Good(TestGop->Blt(TestGop,Out,EfiBltVideoToBltBuffer,200,340,0,0,8,8,0),"M4 OVERLAP LEFT READ FAILED");
 for(Y=0;Y<8;Y++) for(X=0;X<8;X++) Check(Same(Out[Y*8+X],Source[Y*10+X]),"M4 OVERLAP LEFT DATA FAILED");
 /* Reject invalid inputs before any framebuffer or caller-buffer access. */
 Check(TestGop->Blt(TestGop,NULL,EfiBltVideoFill,0,0,0,0,1,1,0)==EFI_INVALID_PARAMETER &&
  TestGop->Blt(TestGop,&Pixel,EfiBltVideoFill,0,0,MAX_UINTN,0,2,1,0)==EFI_INVALID_PARAMETER &&
  TestGop->Blt(TestGop,&Pixel,EfiBltVideoFill,0,0,0,0,0,1,0)==EFI_INVALID_PARAMETER &&
  TestGop->Blt(TestGop,Source,EfiBltBufferToVideo,1,0,0,0,8,8,32)==EFI_INVALID_PARAMETER &&
  TestGop->Blt(TestGop,Source,EfiBltBufferToVideo,0,MAX_UINTN,0,0,1,1,0)==EFI_INVALID_PARAMETER &&
  TestGop->Blt(TestGop,Out,EfiBltVideoToBltBuffer,TestH->width,0,0,0,1,1,0)==EFI_INVALID_PARAMETER &&
  TestGop->Blt(TestGop,NULL,EfiBltVideoToVideo,0,0,TestH->width,0,1,1,0)==EFI_INVALID_PARAMETER &&
  TestGop->Blt(TestGop,(VOID *)MAX_UINTN,EfiBltBufferToVideo,0,0,0,0,1,1,0)==EFI_INVALID_PARAMETER &&
  TestGop->Blt(TestGop,&Pixel,EfiGraphicsOutputBltOperationMax,0,0,0,0,1,1,0)==EFI_INVALID_PARAMETER,"M4 INVALID BLT FAILED");
 /* A visible RGB chart is produced exclusively through the new X64 GOP. */
 Pixel=Color(255,0,0);Good(TestGop->Blt(TestGop,&Pixel,EfiBltVideoFill,0,0,16,260,96,48,0),"M4 RED BAR FAILED");
 Pixel=Color(0,255,0);Good(TestGop->Blt(TestGop,&Pixel,EfiBltVideoFill,0,0,120,260,96,48,0),"M4 GREEN BAR FAILED");
 Pixel=Color(0,0,255);Good(TestGop->Blt(TestGop,&Pixel,EfiBltVideoFill,0,0,224,260,96,48,0),"M4 BLUE BAR FAILED");
 DrawText(0,"M4 EDK2 GOP X64 OK");DrawText(1,"QUERY SET MODE OK");DrawText(2,"ALL FOUR BLT OPS OK");
 DrawText(3,"PIXEL READBACK OK");DrawText(4,"OVERLAP BOUNDS TEST OK");DrawText(5,"M3 BOOT SERVICES OK");
 BaytrailHalt();return EFI_SUCCESS;
}
