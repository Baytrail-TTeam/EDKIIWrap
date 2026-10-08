/* Fixed-mode GOP over an inherited framebuffer. No PCI/GPU register access.
 * Pixel conversion/fill uses EDK2 FrameBufferBltLib; video copies use overlap-
 * safe BaseMemoryLib rows to avoid the upstream descending-row off-by-one.
 */
#include <PiDxe.h>
#include <Guid/GraphicsInfoHob.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/DevicePath.h>
#include <Library/HobLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/FrameBufferBltLib.h>
#include "M3Handoff.h"
#include "Marker.h"
extern EFI_GUID gBaytrailHandoffGuid;
STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL Gop;
STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE Mode;
STATIC EFI_GRAPHICS_OUTPUT_MODE_INFORMATION Info;
STATIC FRAME_BUFFER_CONFIGURE *Config;
STATIC EFI_HANDLE Handle;
STATIC struct {
 VENDOR_DEVICE_PATH Vendor;
 EFI_DEVICE_PATH_PROTOCOL End;
} Path = {
 {{HARDWARE_DEVICE_PATH,HW_VENDOR_DP,{sizeof(VENDOR_DEVICE_PATH),0}},
  {0x482d8863,0x48f7,0x4764,{0xb1,0xa0,0x95,0x22,0xbf,0x0e,0x57,0xf3}}},
 {END_DEVICE_PATH_TYPE,END_ENTIRE_DEVICE_PATH_SUBTYPE,{sizeof(EFI_DEVICE_PATH_PROTOCOL),0}}
};
STATIC BOOLEAN Rect(UINTN X,UINTN Y,UINTN W,UINTN H) {
 return X<Info.HorizontalResolution && Y<Info.VerticalResolution &&
  W<=Info.HorizontalResolution-X && H<=Info.VerticalResolution-Y;
}
STATIC BOOLEAN BufferRect(EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Buffer,UINTN X,UINTN Y,
                          UINTN W,UINTN H,UINTN *Delta) {
 UINTN End;
 if(!Buffer || X>MAX_UINTN-W || X+W>MAX_UINTN/4 || Y>MAX_UINTN-H) return FALSE;
 if(!*Delta) *Delta=W*4;
 if(*Delta<(X+W)*4 || Y+H-1>MAX_UINTN/ *Delta) return FALSE;
 End=(Y+H-1)* *Delta;
 if(End>MAX_UINTN-(X+W)*4) return FALSE;
 End+=(X+W)*4;
 return (UINTN)Buffer<=MAX_UINTN-End;
}
STATIC EFI_STATUS EFIAPI Query(EFI_GRAPHICS_OUTPUT_PROTOCOL *This,UINT32 Number,
 UINTN *Size,EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Result) {
 EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Copy;
 if(This!=&Gop || Number!=0 || !Size || !Result) return EFI_INVALID_PARAMETER;
 Copy=AllocateCopyPool(sizeof(Info),&Info);
 if(!Copy) return EFI_OUT_OF_RESOURCES;
 *Size=sizeof(Info);*Result=Copy;return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI Blt(EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
 EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Buffer,EFI_GRAPHICS_OUTPUT_BLT_OPERATION Operation,
 UINTN Sx,UINTN Sy,UINTN Dx,UINTN Dy,UINTN W,UINTN H,UINTN Delta) {
 EFI_TPL Old;RETURN_STATUS Status=RETURN_SUCCESS;UINTN Row,Index,Stride;
 UINT8 *Fb=(VOID *)(UINTN)Mode.FrameBufferBase;
 if(This!=&Gop || !W || !H || Operation>=EfiGraphicsOutputBltOperationMax)
  return EFI_INVALID_PARAMETER;
 switch(Operation) {
 case EfiBltVideoFill:
  if(!Buffer || !Rect(Dx,Dy,W,H)) return EFI_INVALID_PARAMETER;break;
 case EfiBltBufferToVideo:
  if(!Rect(Dx,Dy,W,H) || !BufferRect(Buffer,Sx,Sy,W,H,&Delta)) return EFI_INVALID_PARAMETER;break;
 case EfiBltVideoToBltBuffer:
  if(!Rect(Sx,Sy,W,H) || !BufferRect(Buffer,Dx,Dy,W,H,&Delta)) return EFI_INVALID_PARAMETER;break;
 case EfiBltVideoToVideo:
  if(!Rect(Sx,Sy,W,H) || !Rect(Dx,Dy,W,H)) return EFI_INVALID_PARAMETER;break;
 default:return EFI_INVALID_PARAMETER;
 }
 Old=gBS->RaiseTPL(TPL_NOTIFY);
 if(Operation==EfiBltVideoToVideo) {
  Stride=(UINTN)Info.PixelsPerScanLine*4;
  for(Index=0;Index<H;Index++) {
   Row=Dy>Sy?H-1-Index:Index;
   CopyMem(Fb+(Dy+Row)*Stride+Dx*4,Fb+(Sy+Row)*Stride+Sx*4,W*4);
  }
 } else Status=FrameBufferBlt(Config,Buffer,Operation,Sx,Sy,Dx,Dy,W,H,Delta);
 gBS->RestoreTPL(Old);
 return RETURN_ERROR(Status)?EFI_INVALID_PARAMETER:EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI Set(EFI_GRAPHICS_OUTPUT_PROTOCOL *This,UINT32 Number) {
 EFI_GRAPHICS_OUTPUT_BLT_PIXEL Black={0,0,0,0};EFI_STATUS Status;
 if(This!=&Gop) return EFI_INVALID_PARAMETER;
 if(Number!=0) return EFI_UNSUPPORTED;
 Status=Blt(This,&Black,EfiBltVideoFill,0,0,0,0,Info.HorizontalResolution,Info.VerticalResolution,0);
 if(!EFI_ERROR(Status)) Mode.Mode=0;
 return Status;
}
STATIC BOOLEAN Mask8(UINT32 M) {
 if(!M) return FALSE;
 while(!(M&1)) M>>=1;
 return M==255;
}
EFI_STATUS EFIAPI GopEntry(EFI_HANDLE Image,EFI_SYSTEM_TABLE *St) {
 VOID *Gh=GetFirstGuidHob(&gEfiGraphicsInfoHobGuid),*Mh=GetFirstGuidHob(&gBaytrailHandoffGuid);
 EFI_PEI_GRAPHICS_INFO_HOB *G;M3Handoff *M;Handoff *H;
 EFI_STATUS Status;RETURN_STATUS R;UINTN Size=0;UINT64 Span;UINT32 Red,Green,Blue,Reserved;
 (VOID)Image;(VOID)St;
 if(!Gh || !Mh || GET_GUID_HOB_DATA_SIZE(Gh)!=sizeof(*G) || GET_GUID_HOB_DATA_SIZE(Mh)!=sizeof(*M)) return EFI_NOT_FOUND;
 G=GET_GUID_HOB_DATA(Gh);M=GET_GUID_HOB_DATA(Mh);H=&M->common;Info=G->GraphicsMode;
 BaytrailMarker(H,7,"M4 GOP DRIVER STARTED",0xffffff);
 Span=(UINT64)Info.PixelsPerScanLine*Info.VerticalResolution;
 if(Span>MAX_UINT32/4) goto Unsupported;
 Span*=4;
 if(!Info.HorizontalResolution || !Info.VerticalResolution || Info.HorizontalResolution>16384 ||
    Info.PixelsPerScanLine<Info.HorizontalResolution || !G->FrameBufferBase ||
    G->FrameBufferBase>=0x100000000ULL || Span>G->FrameBufferSize ||
    G->FrameBufferSize>0x100000000ULL-G->FrameBufferBase ||
    G->FrameBufferBase!=H->framebuffer || G->FrameBufferSize!=H->framebuffer_size ||
    Info.HorizontalResolution!=H->width || Info.VerticalResolution!=H->height ||
    Info.PixelsPerScanLine!=H->stride || Info.PixelFormat!=H->pixel_format || Info.PixelFormat>PixelBitMask)
  goto Unsupported;
 if(Info.PixelFormat==PixelBitMask) {
  Red=Info.PixelInformation.RedMask;Green=Info.PixelInformation.GreenMask;
  Blue=Info.PixelInformation.BlueMask;Reserved=Info.PixelInformation.ReservedMask;
  /* This M4 supports 32 bpp with contiguous 8-bit channels only. */
  if(!Mask8(Red)||!Mask8(Green)||!Mask8(Blue)|| (Red&Green)||(Red&Blue)||(Green&Blue)||
     ((Red|Green|Blue)&Reserved) || (Red|Green|Blue|Reserved)!=MAX_UINT32) goto Unsupported;
 }
 R=FrameBufferBltConfigure((VOID *)(UINTN)G->FrameBufferBase,&Info,NULL,&Size);
 if(R!=RETURN_BUFFER_TOO_SMALL) goto Unsupported;
 Config=AllocatePool(Size);if(!Config) {Status=EFI_OUT_OF_RESOURCES;goto Failed;}
 R=FrameBufferBltConfigure((VOID *)(UINTN)G->FrameBufferBase,&Info,Config,&Size);
 if(RETURN_ERROR(R)) {Status=EFI_UNSUPPORTED;goto Free;}
 Mode.MaxMode=1;Mode.Mode=0;Mode.Info=&Info;Mode.SizeOfInfo=sizeof(Info);
 Mode.FrameBufferBase=G->FrameBufferBase;Mode.FrameBufferSize=G->FrameBufferSize;
 Gop.QueryMode=Query;Gop.SetMode=Set;Gop.Blt=Blt;Gop.Mode=&Mode;
 Status=gBS->InstallMultipleProtocolInterfaces(&Handle,&gEfiGraphicsOutputProtocolGuid,&Gop,
  &gEfiDevicePathProtocolGuid,&Path,NULL);
 if(EFI_ERROR(Status)) goto Free;
 BaytrailMarker(H,7,"M4 GOP INSTALLED",0xffffff);return EFI_SUCCESS;
 Unsupported:Status=EFI_UNSUPPORTED;goto Failed;
 Free:FreePool(Config);Config=NULL;
 Failed:BaytrailMarker(H,7,"M4 GOP INSTALL FAILED",0xff4040);return Status;
}
