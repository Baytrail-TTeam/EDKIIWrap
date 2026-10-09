/* A deliberately restricted PCI I/O provider for one already assigned xHCI.
 * No PCI bus reallocation, no GPU/eMMC binding, identity DMA below 4 GiB.
 * Only the tablet's 8086:0f35 and QEMU's 1b36:000d are accepted. */
#include <Uefi.h>
#include <Protocol/PciIo.h>
#include <Protocol/DevicePath.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include "M3Handoff.h"
#include "Marker.h"
#include <Library/PrintLib.h>
STATIC EFI_PCI_IO_PROTOCOL Pci;
STATIC M3Handoff *HandoffM;
STATIC UINTN Device,Function;
STATIC UINT64 Bar,Span,Attributes;
STATIC UINT32 Legacy;
STATIC struct {VENDOR_DEVICE_PATH Root;PCI_DEVICE_PATH Pci;EFI_DEVICE_PATH_PROTOCOL End;} Path={
 {{HARDWARE_DEVICE_PATH,HW_VENDOR_DP,{sizeof(VENDOR_DEVICE_PATH),0}},
 {0x9fe49a01,0x1671,0x48d1,{0xb5,0xa1,0x39,0x26,0xc5,0xac,0xee,0x02}}},
 {{HARDWARE_DEVICE_PATH,HW_PCI_DP,{sizeof(PCI_DEVICE_PATH),0}},0,0},
 {END_DEVICE_PATH_TYPE,END_ENTIRE_DEVICE_PATH_SUBTYPE,{sizeof(EFI_DEVICE_PATH_PROTOCOL),0}}};
STATIC UINT32 Config(UINTN Dev,UINTN Fun,UINT32 Offset) {
 IoWrite32(0xcf8,0x80000000|((UINT32)Dev<<11)|((UINT32)Fun<<8)|(Offset&0xfc));return IoRead32(0xcfc);
}
STATIC UINT16 Command(VOID) {return (UINT16)Config(Device,Function,4);}
STATIC VOID WriteCommand(UINT16 Value) {
 IoWrite32(0xcf8,0x80000000|((UINT32)Device<<11)|((UINT32)Function<<8)|4);IoWrite16(0xcfc,Value);
}
STATIC EFI_STATUS EFIAPI ConfigRead(EFI_PCI_IO_PROTOCOL *This,EFI_PCI_IO_PROTOCOL_WIDTH W,UINT32 Offset,UINTN Count,VOID *Buffer) {
 UINTN I,Size;UINT32 Value;
 if(This!=&Pci || !Buffer || W>EfiPciIoWidthUint32) return EFI_INVALID_PARAMETER;
 Size=(UINTN)1<<W;if(Offset%Size || Offset>256 || Count>(256-Offset)/Size) return EFI_UNSUPPORTED;
 for(I=0;I<Count;I++) {Value=Config(Device,Function,Offset)>>((Offset&3)*8);CopyMem((UINT8 *)Buffer+I*Size,&Value,Size);Offset+=(UINT32)Size;}
 return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI ConfigWrite(EFI_PCI_IO_PROTOCOL *This,EFI_PCI_IO_PROTOCOL_WIDTH W,UINT32 Offset,UINTN Count,VOID *Buffer) {
 (VOID)This;(VOID)W;(VOID)Offset;(VOID)Count;(VOID)Buffer;return EFI_UNSUPPORTED;
}
STATIC EFI_STATUS MemAccess(BOOLEAN Write,EFI_PCI_IO_PROTOCOL *This,EFI_PCI_IO_PROTOCOL_WIDTH W,UINT8 Index,UINT64 Offset,UINTN Count,VOID *Buffer) {
 UINTN Size,I,AddressStep,BufferStep;UINT64 Value=0;UINT8 *Bytes=Buffer;
 if(This!=&Pci || !Buffer || W>=EfiPciIoWidthMaximum || Index!=0) return EFI_INVALID_PARAMETER;
 Size=(UINTN)1<<(W&3);AddressStep=(W>=EfiPciIoWidthFifoUint8 && W<=EfiPciIoWidthFifoUint64)?0:Size;
 BufferStep=(W>=EfiPciIoWidthFillUint8)?0:Size;
 if(Offset%Size || Offset>=Span || (Count && (Size>Span-Offset || (AddressStep && Count>(Span-Offset)/Size)))) return EFI_UNSUPPORTED;
 for(I=0;I<Count;I++) {
  UINTN Address=(UINTN)(Bar+Offset);if(Write) CopyMem(&Value,Bytes,Size);
  if(Write) {
   /* Keep the OS-style ownership obtained from stock firmware, even though
    * the unmodified EDK2 pre-OS driver requests BIOS-style ownership. */
   if(Legacy && Offset==Legacy && Size==4) Value=(Value&~BIT16)|BIT24;
   switch(Size) {case 1:MmioWrite8(Address,(UINT8)Value);break;case 2:MmioWrite16(Address,(UINT16)Value);break;case 4:MmioWrite32(Address,(UINT32)Value);break;case 8:MmioWrite64(Address,Value);break;}
  } else {
   switch(Size) {case 1:Value=MmioRead8(Address);break;case 2:Value=MmioRead16(Address);break;case 4:Value=MmioRead32(Address);break;case 8:Value=MmioRead64(Address);break;}
   CopyMem(Bytes,&Value,Size);
  }
  Offset+=AddressStep;Bytes+=BufferStep;
 }
 return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI MemRead(EFI_PCI_IO_PROTOCOL *T,EFI_PCI_IO_PROTOCOL_WIDTH W,UINT8 B,UINT64 O,UINTN C,VOID *P) {return MemAccess(FALSE,T,W,B,O,C,P);}
STATIC EFI_STATUS EFIAPI MemWrite(EFI_PCI_IO_PROTOCOL *T,EFI_PCI_IO_PROTOCOL_WIDTH W,UINT8 B,UINT64 O,UINTN C,VOID *P) {return MemAccess(TRUE,T,W,B,O,C,P);}
STATIC EFI_STATUS EFIAPI NoIo(EFI_PCI_IO_PROTOCOL *T,EFI_PCI_IO_PROTOCOL_WIDTH W,UINT8 B,UINT64 O,UINTN C,VOID *P) {(VOID)T;(VOID)W;(VOID)B;(VOID)O;(VOID)C;(VOID)P;return EFI_UNSUPPORTED;}
STATIC EFI_STATUS EFIAPI NoPoll(EFI_PCI_IO_PROTOCOL *T,EFI_PCI_IO_PROTOCOL_WIDTH W,UINT8 B,UINT64 O,UINT64 M,UINT64 V,UINT64 D,UINT64 *R) {(VOID)T;(VOID)W;(VOID)B;(VOID)O;(VOID)M;(VOID)V;(VOID)D;(VOID)R;return EFI_UNSUPPORTED;}
STATIC EFI_STATUS EFIAPI NoCopy(EFI_PCI_IO_PROTOCOL *T,EFI_PCI_IO_PROTOCOL_WIDTH W,UINT8 DB,UINT64 DO,UINT8 SB,UINT64 SO,UINTN C) {(VOID)T;(VOID)W;(VOID)DB;(VOID)DO;(VOID)SB;(VOID)SO;(VOID)C;return EFI_UNSUPPORTED;}
STATIC BOOLEAN Owned(UINT64 Address,UINTN Size) {
 UINT64 End=HandoffM->arena_base+HandoffM->arena_size;
 return Address>=HandoffM->arena_base && Address<End && Size<=End-Address;
}
STATIC EFI_STATUS EFIAPI Allocate(EFI_PCI_IO_PROTOCOL *T,EFI_ALLOCATE_TYPE Type,EFI_MEMORY_TYPE MemoryType,UINTN Pages,VOID **Host,UINT64 Attr) {
 EFI_PHYSICAL_ADDRESS Address;EFI_STATUS S;(VOID)Type;
 if(T!=&Pci || !Host || !Pages || Pages>HandoffM->arena_size/4096 || Attr ||
    (MemoryType!=EfiBootServicesData && MemoryType!=EfiRuntimeServicesData)) return EFI_INVALID_PARAMETER;
 /* Constrain selection before allocating: child EFI applications can change
  * the free map, and the highest page below 4 GiB need not be in our arena. */
 Address=HandoffM->arena_base+HandoffM->arena_size-1;
 S=gBS->AllocatePages(AllocateMaxAddress,MemoryType,Pages,&Address);if(EFI_ERROR(S)) return S;
 if(!Owned(Address,Pages*4096)) {gBS->FreePages(Address,Pages);return EFI_UNSUPPORTED;}
 *Host=(VOID *)(UINTN)Address;return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI Free(EFI_PCI_IO_PROTOCOL *T,UINTN Pages,VOID *Host) {
 if(T!=&Pci || !Pages || Pages>HandoffM->arena_size/4096 || !Owned((UINTN)Host,Pages*4096)) return EFI_INVALID_PARAMETER;
 return gBS->FreePages((UINTN)Host,Pages);
}
typedef struct {UINT32 Signature;EFI_PCI_IO_PROTOCOL_OPERATION Op;VOID *Host,*Bounce;UINTN Bytes,Pages;} DMA_MAP;
STATIC EFI_STATUS MapBuffer(EFI_PCI_IO_PROTOCOL *T,EFI_PCI_IO_PROTOCOL_OPERATION Op,VOID *Host,UINTN *Bytes,EFI_PHYSICAL_ADDRESS *Address,VOID **Mapping) {
 DMA_MAP *M;EFI_STATUS S;
 if(T!=&Pci || Op>EfiPciIoOperationBusMasterCommonBuffer || !Host || !Bytes || !*Bytes ||
    *Bytes>HandoffM->arena_size || !Address || !Mapping) return EFI_INVALID_PARAMETER;
 if((UINTN)Host>=0x100000000ULL || *Bytes>0x100000000ULL-(UINTN)Host) return EFI_UNSUPPORTED;
 *Mapping=NULL;
 if(Op==EfiPciIoOperationBusMasterCommonBuffer) {
  if(!Owned((UINTN)Host,*Bytes)) return EFI_UNSUPPORTED;*Address=(UINTN)Host;MemoryFence();return EFI_SUCCESS;
 }
 M=AllocateZeroPool(sizeof(*M));if(!M) return EFI_OUT_OF_RESOURCES;
 M->Signature=SIGNATURE_32('D','M','A','5');M->Op=Op;M->Host=Host;M->Bytes=*Bytes;M->Pages=EFI_SIZE_TO_PAGES(*Bytes);
 S=Allocate(&Pci,AllocateAnyPages,EfiBootServicesData,M->Pages,&M->Bounce,0);
 if(EFI_ERROR(S)) {FreePool(M);return S;}
 if(Op==EfiPciIoOperationBusMasterRead) CopyMem(M->Bounce,Host,*Bytes);else ZeroMem(M->Bounce,M->Pages*4096);
 *Address=(UINTN)M->Bounce;*Mapping=M;MemoryFence();return EFI_SUCCESS;
}
/* Latch the first runtime mapping error before xHCI retries overwrite it.
 * Bounds self-tests run before this diagnostic is armed. No failed mapping
 * is submitted to the controller. This build deliberately stops on error. */
STATIC BOOLEAN HoldMapFailure;
STATIC EFI_STATUS EFIAPI Map(EFI_PCI_IO_PROTOCOL *T,EFI_PCI_IO_PROTOCOL_OPERATION Op,VOID *Host,UINTN *Bytes,EFI_PHYSICAL_ADDRESS *Address,VOID **Mapping) {
 EFI_STATUS S=MapBuffer(T,Op,Host,Bytes,Address,Mapping);CHAR8 Text[112];
 if(EFI_ERROR(S) && HoldMapFailure) {
  BaytrailMarker(&HandoffM->common,7,"USB DMA MAP FAILED",0xff4040);
  AsciiSPrint(Text,sizeof(Text),"STATUS %LX OP %u",(UINT64)S,(UINT32)Op);
  BaytrailMarker(&HandoffM->common,8,Text,0xffffff);
  AsciiSPrint(Text,sizeof(Text),"HOST %LX BYTES %LX",(UINT64)(UINTN)Host,Bytes?(UINT64)*Bytes:0);
  BaytrailMarker(&HandoffM->common,9,Text,0xffffff);
  AsciiSPrint(Text,sizeof(Text),"CR3 %LX ORIGINAL %LX",AsmReadCr3(),HandoffM->common.page_tables);
  BaytrailMarker(&HandoffM->common,10,Text,0xffffff);
  AsciiSPrint(Text,sizeof(Text),"ARENA %LX SIZE %LX",HandoffM->arena_base,HandoffM->arena_size);
  BaytrailMarker(&HandoffM->common,11,Text,0xffffff);
  BaytrailMarker(&HandoffM->common,12,"DIAGNOSTIC STOP - PHOTOGRAPH SCREEN",0xffa040);
  BaytrailHalt();
 }
 return S;
}
STATIC EFI_STATUS EFIAPI Unmap(EFI_PCI_IO_PROTOCOL *T,VOID *Mapping) {
 DMA_MAP *M=Mapping;if(T!=&Pci) return EFI_INVALID_PARAMETER;if(!M) {MemoryFence();return EFI_SUCCESS;}
 if(M->Signature!=SIGNATURE_32('D','M','A','5')) return EFI_INVALID_PARAMETER;MemoryFence();
 if(M->Op==EfiPciIoOperationBusMasterWrite) CopyMem(M->Host,M->Bounce,M->Bytes);
 Free(&Pci,M->Pages,M->Bounce);M->Signature=0;FreePool(M);return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI Flush(EFI_PCI_IO_PROTOCOL *T) {if(T!=&Pci) return EFI_INVALID_PARAMETER;MemoryFence();Config(Device,Function,0);return EFI_SUCCESS;}
STATIC EFI_STATUS EFIAPI Location(EFI_PCI_IO_PROTOCOL *T,UINTN *Seg,UINTN *Bus,UINTN *Dev,UINTN *Fun) {
 if(T!=&Pci || !Seg || !Bus || !Dev || !Fun) return EFI_INVALID_PARAMETER;*Seg=0;*Bus=0;*Dev=Device;*Fun=Function;return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI Attribute(EFI_PCI_IO_PROTOCOL *T,EFI_PCI_IO_PROTOCOL_ATTRIBUTE_OPERATION Op,UINT64 Value,UINT64 *Result) {
 UINT64 Supported=EFI_PCI_DEVICE_ENABLE,New;UINT16 Cmd;
 if(T!=&Pci) return EFI_INVALID_PARAMETER;
 if(Op==EfiPciIoAttributeOperationGet || Op==EfiPciIoAttributeOperationSupported) {
  if(!Result) return EFI_INVALID_PARAMETER;*Result=Op==EfiPciIoAttributeOperationGet?Attributes:Supported;return EFI_SUCCESS;
 }
 if(Value&~Supported) return EFI_UNSUPPORTED;
 if(Op==EfiPciIoAttributeOperationSet) New=Value;
 else if(Op==EfiPciIoAttributeOperationEnable) New=Attributes|Value;
 else if(Op==EfiPciIoAttributeOperationDisable) New=Attributes&~Value;
 else return EFI_INVALID_PARAMETER;
 Cmd=Command()&~7; if(New&EFI_PCI_IO_ATTRIBUTE_IO) Cmd|=1;
 if(New&EFI_PCI_IO_ATTRIBUTE_MEMORY) Cmd|=2;if(New&EFI_PCI_IO_ATTRIBUTE_BUS_MASTER) Cmd|=4;
 WriteCommand(Cmd);Attributes=New;return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI GetBar(EFI_PCI_IO_PROTOCOL *T,UINT8 B,UINT64 *S,VOID **R) {(VOID)T;(VOID)B;(VOID)S;(VOID)R;return EFI_UNSUPPORTED;}
STATIC EFI_STATUS EFIAPI SetBar(EFI_PCI_IO_PROTOCOL *T,UINT64 A,UINT8 B,UINT64 *O,UINT64 *L) {(VOID)T;(VOID)A;(VOID)B;(VOID)O;(VOID)L;return EFI_UNSUPPORTED;}
STATIC EFI_STATUS UncacheBar(VOID) {
 UINT64 Address,Cr3=AsmReadCr3()&~0xfffULL,*Pml4,*Pdpt,*Pd,End=Bar+Span;UINTN I;
 /* Do not change a large-page cache attribute over any owned/system RAM. */
 for(I=0;I<HandoffM->common.memory_map_size;I+=HandoffM->common.descriptor_size) {
  EFI_MEMORY_DESCRIPTOR *D=(VOID *)(UINTN)(HandoffM->common.memory_map+I);
  UINT64 Start=Bar&~0x1fffffULL,Limit=(End+0x1fffff)&~0x1fffffULL;
  if(D->Type!=EfiMemoryMappedIO && D->Type!=EfiReservedMemoryType && D->Type!=EfiMemoryMappedIOPortSpace &&
     D->PhysicalStart<Limit && D->PhysicalStart+(D->NumberOfPages<<12)>Start) return EFI_UNSUPPORTED;
 }
 if(((AsmReadMsr64(0x277)>>24)&255)!=0) return EFI_UNSUPPORTED;
 if(Cr3!=HandoffM->common.page_tables) return EFI_UNSUPPORTED;
 Pml4=(VOID *)(UINTN)Cr3;Pdpt=(VOID *)(UINTN)(Pml4[0]&~0xfffULL);
 AsmWbinvd();
 for(Address=Bar&~0x1fffffULL;Address<End;Address+=0x200000) {
  Pd=(VOID *)(UINTN)(Pdpt[Address>>30]&~0xfffULL);
  if((Pd[(Address>>21)&511]&0x81)!=0x81) return EFI_UNSUPPORTED;
  Pd[(Address>>21)&511]|=BIT3|BIT4;
 }
 AsmWriteCr3(Cr3);return EFI_SUCCESS;
}
STATIC EFI_STATUS PrepareLegacy(VOID) {
 UINT32 Offset=(MmioRead32((UINTN)Bar+0x10)>>16)*4,Value,Next;UINTN Attempts;
 while(Offset) {
  if(Offset>Span-4) return EFI_DEVICE_ERROR;
  Value=MmioRead32((UINTN)Bar+Offset);Next=((Value>>8)&255)*4;
  if((Value&255)==1) {
   if(Offset>Span-8) return EFI_DEVICE_ERROR;Legacy=Offset;
   MmioWrite8((UINTN)Bar+Offset+3,(UINT8)(Value>>24)|1);
   for(Attempts=0;Attempts<1000;Attempts++) {
    if(!(MmioRead32((UINTN)Bar+Offset)&BIT16)) break;
    MicroSecondDelay(1000);
   }
   if(Attempts==1000) return EFI_TIMEOUT;
   /* xHCI 1.2b 7.1.2: disable the five controller-local SMI sources,
    * preserve reserved bits, acknowledge the three RW1C status bits. */
   Value=MmioRead32((UINTN)Bar+Offset+4);
   Value=(Value&~(BIT0|BIT4|BIT13|BIT14|BIT15))|BIT29|BIT30|BIT31;
   MmioWrite32((UINTN)Bar+Offset+4,Value);
  }
  if(!Next) break;
  if(Next>Span-Offset-4) return EFI_DEVICE_ERROR;Offset+=Next;
 }
 return EFI_SUCCESS;
}
STATIC EFI_STATUS CheckDmaAndBounds(VOID) {
 UINT32 Value=0x12345678,ReadBack=0;UINTN Bytes=sizeof(Value);EFI_PHYSICAL_ADDRESS Address;VOID *Mapping;
 EFI_STATUS S;
 if(MemRead(&Pci,EfiPciIoWidthUint32,0,Span,1,&ReadBack)!=EFI_UNSUPPORTED ||
    ConfigRead(&Pci,EfiPciIoWidthUint32,256,1,&ReadBack)!=EFI_UNSUPPORTED ||
    Map(&Pci,EfiPciIoOperationBusMasterCommonBuffer,(VOID *)0x1000,&Bytes,&Address,&Mapping)!=EFI_UNSUPPORTED) return EFI_DEVICE_ERROR;
 S=Map(&Pci,EfiPciIoOperationBusMasterRead,&Value,&Bytes,&Address,&Mapping);if(EFI_ERROR(S)) return S;
 CopyMem(&ReadBack,(VOID *)(UINTN)Address,sizeof(Value));Unmap(&Pci,Mapping);
 if(ReadBack!=Value) return EFI_DEVICE_ERROR;
 Bytes=sizeof(Value);S=Map(&Pci,EfiPciIoOperationBusMasterWrite,&Value,&Bytes,&Address,&Mapping);if(EFI_ERROR(S)) return S;
 ReadBack=0x87654321;CopyMem((VOID *)(UINTN)Address,&ReadBack,sizeof(ReadBack));Unmap(&Pci,Mapping);
 return Value==ReadBack?EFI_SUCCESS:EFI_DEVICE_ERROR;
}
EFI_STATUS PublishXhci(M3Handoff *M) {
 UINTN D,F,Found=0;UINT32 Id,Low,High;UINT16 Cmd;EFI_HANDLE Handle=NULL;EFI_STATUS S;
 HandoffM=M;
 for(D=0;D<M->common.table_count;D++) if(M->common.tables[D].signature==SIGNATURE_32('D','M','A','R')) return EFI_UNSUPPORTED;
 for(D=0;D<32;D++) for(F=0;F<8;F++) {
  Id=Config(D,F,0);if(Id==MAX_UINT32 || (Config(D,F,8)>>8)!=0x0c0330) continue;
  if(Id!=0x0f358086 && Id!=0x000d1b36) continue;
  if(Found++) return EFI_UNSUPPORTED;Device=D;Function=F;Span=Id==0x0f358086?0x10000:0x4000;
 }
 if(!Found) return EFI_NOT_FOUND;
 Low=Config(Device,Function,0x10);High=(Low&6)==4?Config(Device,Function,0x14):0;
 if((Low&1) || High || !(Low&~15U)) return EFI_UNSUPPORTED;
 Bar=Low&~15U;if(Bar+Span>0x100000000ULL || (Bar&(Span-1))) return EFI_UNSUPPORTED;
 S=UncacheBar();if(EFI_ERROR(S)) return S;
 Cmd=Command();if(!(Cmd&2)) return EFI_NOT_READY;
 /* Validate layout before exposing it to the official driver. */
 if(MmioRead8((UINTN)Bar)<0x20 || MmioRead8((UINTN)Bar)>0x80 ||
    MmioRead32((UINTN)Bar+0x14)>=Span || (MmioRead32((UINTN)Bar+0x18)&~31U)>=Span ||
    (UINT64)MmioRead8((UINTN)Bar)+0x400+((MmioRead32((UINTN)Bar+4)>>24)&255)*16>Span) return EFI_DEVICE_ERROR;
 Attributes=((Cmd&1)?EFI_PCI_IO_ATTRIBUTE_IO:0)|((Cmd&2)?EFI_PCI_IO_ATTRIBUTE_MEMORY:0)|((Cmd&4)?EFI_PCI_IO_ATTRIBUTE_BUS_MASTER:0);
 S=PrepareLegacy();if(EFI_ERROR(S)) return S;
 /* Suppress PCI INTx; this variant keeps CPU IRQs disabled and polls. */
 WriteCommand(Cmd|BIT10);
 Pci.PollMem=NoPoll;Pci.PollIo=NoPoll;Pci.Mem.Read=MemRead;Pci.Mem.Write=MemWrite;Pci.Io.Read=NoIo;Pci.Io.Write=NoIo;
 Pci.Pci.Read=ConfigRead;Pci.Pci.Write=ConfigWrite;Pci.CopyMem=NoCopy;Pci.Map=Map;Pci.Unmap=Unmap;
 Pci.AllocateBuffer=Allocate;Pci.FreeBuffer=Free;Pci.Flush=Flush;Pci.GetLocation=Location;Pci.Attributes=Attribute;
 Pci.GetBarAttributes=GetBar;Pci.SetBarAttributes=SetBar;Path.Pci.Device=(UINT8)Device;Path.Pci.Function=(UINT8)Function;
 S=CheckDmaAndBounds();if(EFI_ERROR(S)) return S;
 HoldMapFailure=TRUE;
 return gBS->InstallMultipleProtocolInterfaces(&Handle,&gEfiPciIoProtocolGuid,&Pci,&gEfiDevicePathProtocolGuid,&Path,NULL);
}
