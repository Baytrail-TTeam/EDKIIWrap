#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/LoadedImage.h>
#include <Guid/Acpi.h>
#include <Guid/SmBios.h>
#include "Handoff.h"
#if RUN_TRANSITION == 2
#include "M3Handoff.h"
#include <Library/PeCoffLib.h>
#include <IndustryStandard/PeImage.h>
#endif
#if RUN_TRANSITION && !defined(BAYTRAIL_BLOBS_PREINCLUDED)
#include "blobs.h"
#endif
static EFI_SYSTEM_TABLE *st;
static EFI_BOOT_SERVICES *bs;
static EFI_GUID gop_guid=EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
static EFI_GUID loaded_guid=EFI_LOADED_IMAGE_PROTOCOL_GUID;
static EFI_GUID acpi1_guid=ACPI_TABLE_GUID,acpi2_guid=EFI_ACPI_TABLE_GUID;
static EFI_GUID smbios2_guid=SMBIOS_TABLE_GUID,smbios3_guid=SMBIOS3_TABLE_GUID;
static void zero(void*p,UINTN n) { UINT8 *b=p; while(n--) *b++=0; }
static void copy(void*d,const void*s,UINTN n) { UINT8*a=d; const UINT8*b=s; while(n--) *a++=*b++; }
static int equal(const void*a,const void*b,UINTN n) {
 const UINT8*x=a,*y=b; while(n--) if(*x++!=*y++) return 0; return 1;
}
static void say(CHAR16 *s) { st->ConOut->OutputString(st->ConOut,s); }
static void hex(uint64_t x) {
 CHAR16 s[19]; s[0]='0';s[1]='x';
 for(unsigned i=0;i<16;i++) s[2+i]=L"0123456789ABCDEF"[(x>>((15-i)*4))&15];
 s[18]=0;say(s);
}
static void line(CHAR16*s,uint64_t x) {say(s);hex(x);say(L"\r\n");}
static EFI_STATUS pages(UINTN n, EFI_MEMORY_TYPE type, uint64_t *out) {
 EFI_PHYSICAL_ADDRESS p=0xffffefff; /* keep exclusive stack top below 4 GiB */
 EFI_STATUS status=bs->AllocatePages(AllocateMaxAddress,type,n,&p);
 if(!EFI_ERROR(status)) {zero((void*)(UINTN)p,n*4096);*out=p;}
 return status;
}

#if RUN_TRANSITION == 2
typedef struct { const unsigned char *bytes; UINTN length; } PeSource;
static RETURN_STATUS EFIAPI pe_read(void *handle,UINTN offset,UINTN *size,void *buffer) {
 PeSource *source=handle;
 if(offset>source->length || *size>source->length-offset) return RETURN_LOAD_ERROR;
 copy(buffer,source->bytes+offset,*size);return RETURN_SUCCESS;
}
static EFI_STATUS load_m3_entry(uint64_t *base,UINTN *count,uint64_t *entry) {
 PE_COFF_LOADER_IMAGE_CONTEXT context;PeSource source={payload_blob,sizeof(payload_blob)};
 zero(&context,sizeof(context));context.Handle=&source;context.ImageRead=pe_read;
 EFI_STATUS status=PeCoffLoaderGetImageInfo(&context);
 if(EFI_ERROR(status)) return status;
 if(context.Machine!=IMAGE_FILE_MACHINE_X64 || context.RelocationsStripped ||
    !context.ImageSize || context.ImageSize>16*1024*1024 ||
    !context.SectionAlignment || context.SectionAlignment>4096 ||
    (context.SectionAlignment&(context.SectionAlignment-1))) return EFI_UNSUPPORTED;
 *count=(UINTN)((context.ImageSize+4095)>>12);
 status=pages(*count,EfiLoaderCode,base);if(EFI_ERROR(status)) return status;
 context.ImageAddress=*base;
 status=PeCoffLoaderLoadImage(&context);if(EFI_ERROR(status)) return status;
 status=PeCoffLoaderRelocateImage(&context);if(EFI_ERROR(status)) return status;
 if(context.EntryPoint<*base || context.EntryPoint-*base>=context.ImageSize) return EFI_LOAD_ERROR;
 *entry=context.EntryPoint;return EFI_SUCCESS;
}
#endif

static uint32_t u32(const void*p) {uint32_t x;copy(&x,p,4);return x;}
static uint64_t u64(const void*p) {uint64_t x;copy(&x,p,8);return x;}
static int checksum(const void*p,unsigned n) {
 const UINT8*b=p;UINT8 sum=0;while(n--) sum+=*b++;return sum==0;
}
/* Never dereference MMIO or unlisted table ranges; no >4GiB IA32 dereferences. */
static int ram(Handoff*h,uint64_t p,uint64_t n) {
 if(!p || !n || p>0xffffffff || n>0x100000000ULL-p) return 0;
 for(uint64_t i=0;i<h->memory_map_size;i+=h->descriptor_size) {
  EFI_MEMORY_DESCRIPTOR*d=(void*)(UINTN)(h->memory_map+i);
  if(d->Type==EfiMemoryMappedIO || d->Type==EfiMemoryMappedIOPortSpace ||
     d->Type==EfiUnusableMemory || d->Type==EfiReservedMemoryType) continue;
  uint64_t size=d->NumberOfPages<<12;
  if(p>=d->PhysicalStart && p-d->PhysicalStart<=size && n<=size-(p-d->PhysicalStart)) return 1;
 }
 return 0;
}
static int record(Handoff*h,uint64_t p,int facs) {
 for(unsigned i=0;i<h->table_count;i++) if(h->tables[i].address==p) return 1;
 if(!ram(h,p,36)) return 0;
 UINT8*b=(void*)(UINTN)p;uint32_t n=u32(b+4),sig=u32(b);
 if(n<36 || n>1024*1024 || !ram(h,p,n) || (!facs&&!checksum(b,n)) ||
    h->table_count==MAX_TABLES) return 0;
 h->tables[h->table_count++]=(TableRef){p,n,sig};return 1;
}
static int root(Handoff*h,uint64_t p,int xsdt) {
 if(!p) return 1;
 if(!ram(h,p,36) || u32((void*)(UINTN)p)!=(xsdt?0x54445358u:0x54445352u) || !record(h,p,0)) return 0;
 UINT8*b=(void*)(UINTN)p;unsigned n=u32(b+4),step=xsdt?8:4;
 if((n-36)%step) return 0;
 for(unsigned i=36;i<n;i+=step) {
  uint64_t t=xsdt?u64(b+i):u32(b+i);
  if(!record(h,t,0)) return 0;
  UINT8*f=(void*)(UINTN)t;
  if(u32(f)==0x50434146) {
   unsigned size=u32(f+4);if(size<44) return 0;
   uint64_t facs=u32(f+36),dsdt=u32(f+40);
   if(size>=140 && u64(f+132)) facs=u64(f+132);
   if(size>=148 && u64(f+140)) dsdt=u64(f+140);
   if(!dsdt || !record(h,dsdt,0)) return 0;
   if(facs && (!ram(h,facs,8) || u32((void*)(UINTN)facs)!=0x53434146 || !record(h,facs,1))) return 0;
  }
 }
 return 1;
}
static int acpi(Handoff*h,uint64_t p) {
 if(!p) return 1;
 if(!ram(h,p,20)) return 0;
 UINT8*b=(void*)(UINTN)p;
 if(!equal(b,"RSD PTR ",8)||!checksum(b,20)) return 0;
 if(!root(h,u32(b+16),0)) return 0;
 if(b[15]>=2) {
  if(!ram(h,p,36)) return 0;
  unsigned n=u32(b+20);
  if(n<36 || n>4096 || !ram(h,p,n)||!checksum(b,n)) return 0;
  if(!root(h,u64(b+24),1)) return 0;
 }
 return 1;
}
static int smbios(Handoff*h,uint64_t p,int v3) {
 if(!p) return 1;
 unsigned minimum=v3?24:31;
 if(!ram(h,p,minimum)) return 0;
 UINT8*b=(void*)(UINTN)p;
 unsigned n=b[v3?6:5];
 if(!equal(b,v3?"_SM3_":"_SM_",v3?5:4)||n<minimum||n>64||!ram(h,p,n)||!checksum(b,n)) return 0;
 if(!v3 && (!equal(b+16,"_DMI_",5)||!checksum(b+16,15))) return 0;
 uint64_t data=v3?u64(b+16):u32(b+24);
 uint32_t size=v3?u32(b+12):(uint32_t)(b[22]|b[23]<<8);
 if(!ram(h,data,size)) return 0;
 if(v3) {h->smbios3_data=data;h->smbios3_size=size;}
 else {h->smbios2_data=data;h->smbios2_size=size;}
 return 1;
}
static void cpuid(uint32_t leaf,uint32_t*a,uint32_t*b,uint32_t*c,uint32_t*d) {
 __asm__ volatile("cpuid":"=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d):"a"(leaf),"c"(0));
}
static uint64_t msr(uint32_t n) {
 uint32_t lo,hi;__asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(n));return ((uint64_t)hi<<32)|lo;
}
static int cpu(Handoff*h) {
 uint32_t a,b,c,d;cpuid(1,&a,&b,&c,&d);h->cpuid_signature=a;
 if(!(d&(1u<<6)) || !(d&(1u<<5)) || !(d&(1u<<3))) return 0; /* PAE, MSR, PSE */
 cpuid(0x80000000,&a,&b,&c,&d);if(a<0x80000001) return 0;
 cpuid(0x80000001,&a,&b,&c,&d);if(!(d&(1u<<29))) return 0;
 uint32_t cr0,cr4;__asm__ volatile("mov %%cr0,%0":"=r"(cr0));
 __asm__ volatile("mov %%cr4,%0":"=r"(cr4));
 h->initial_cr0=cr0;h->initial_cr4=cr4;h->initial_efer=msr(0xc0000080);
 cpuid(1,&a,&b,&c,&d);if(!(d&(1u<<16))) return 0; /* PAT */
 h->initial_pat=msr(0x277);
 return (cr0&1) && !(h->initial_efer&(1u<<10)) && ((h->initial_pat>>24)&255)==0;
}
static __attribute__((noreturn,unused)) void stop(void) {for(;;) __asm__ volatile("cli; hlt");}
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image,EFI_SYSTEM_TABLE *system) {
 st=system;bs=st->BootServices;
 EFI_STATUS status;uint64_t hp=0,mp=0;Handoff*h;
 uint64_t tr=0,pl=0,stack=0,pt=0;
 UINTN capacity=65536,key=0,ds=0,sz;UINT32 dv=0;
#if RUN_TRANSITION == 2
 M3Handoff *m3=NULL;uint64_t arena=0,low_arena=0,realmode_arena=0,fv=0,pl_entry=0;UINTN pl_pages=0;
#endif
 say(L"Baytrail X64 PoC / " );
#if RUN_TRANSITION == 2
#if defined(MILESTONE_USB)
 say(L"EDK2 X64 NATIVE USB TEST\r\n");
#elif defined(MILESTONE_SHELL)
 say(L"EDK2 X64 SHELL RAM\r\n");
#elif defined(MILESTONE_4)
 say(L"M4 EDK2 GOP\r\n");
#else
 say(L"M3 EDK2 DXE\r\n");
#endif
#elif RUN_TRANSITION
 say(L"M2 TRANSITION\r\n");
#else
 say(L"M1 COLLECT ONLY\r\n");
#endif
 status=bs->SetWatchdogTimer(0,0,0,NULL);if(EFI_ERROR(status)) return status;
 if(EFI_ERROR(status=pages(1,EfiLoaderData,&hp))) return status;
 h=(void*)(UINTN)hp;h->magic=HANDOFF_MAGIC;h->version=1;h->size=sizeof(*h);
#if RUN_TRANSITION == 2
 m3=(void*)(UINTN)hp;m3->m3_magic=M3_MAGIC;m3->m3_version=M3_VERSION;m3->m3_size=sizeof(*m3);
 m3->firmware_system_table=(UINTN)system;
#endif
 if(!cpu(h)) {say(L"Unsupported CPU/mode/PAT (requires PAT entry 3 = UC).\r\n");status=EFI_UNSUPPORTED;goto cleanup;}
 if(EFI_ERROR(status=pages(16,EfiLoaderData,&mp))) goto cleanup;
 h->memory_map=mp;
 EFI_LOADED_IMAGE_PROTOCOL *loaded=NULL;
 status=bs->HandleProtocol(image,&loaded_guid,(void**)&loaded);
 if(EFI_ERROR(status)) goto cleanup;
 h->loader_base=(UINTN)loaded->ImageBase;h->loader_size=loaded->ImageSize;
 EFI_GRAPHICS_OUTPUT_PROTOCOL*g=NULL;
 status=bs->LocateProtocol(&gop_guid,NULL,(void**)&g);
 if(EFI_ERROR(status)||!g || !g->Mode || !g->Mode->Info) {status=EFI_UNSUPPORTED;goto cleanup;}
 EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *gi=g->Mode->Info;
 h->framebuffer=g->Mode->FrameBufferBase;h->framebuffer_size=g->Mode->FrameBufferSize;
 h->width=gi->HorizontalResolution;h->height=gi->VerticalResolution;
 h->stride=gi->PixelsPerScanLine;h->pixel_format=gi->PixelFormat;
 h->red_mask=gi->PixelInformation.RedMask;h->green_mask=gi->PixelInformation.GreenMask;
 h->blue_mask=gi->PixelInformation.BlueMask;h->reserved_mask=gi->PixelInformation.ReservedMask;
 if(gi->PixelFormat>PixelBitMask || h->width<356 || h->height<64 || h->stride<h->width ||
    !h->framebuffer || h->framebuffer>0xffffffff ||
    h->framebuffer_size>0x100000000ULL-h->framebuffer ||
    (uint64_t)h->stride*h->height>h->framebuffer_size/4) {status=EFI_UNSUPPORTED;goto cleanup;}
 if(gi->PixelFormat==PixelBitMask &&
   (__builtin_popcount(h->red_mask)!=8||__builtin_popcount(h->green_mask)!=8||__builtin_popcount(h->blue_mask)!=8||
    ((h->red_mask|h->green_mask|h->blue_mask)&h->reserved_mask)||
    (h->red_mask&h->green_mask)||(h->red_mask&h->blue_mask)||(h->green_mask&h->blue_mask))) {
   status=EFI_UNSUPPORTED;goto cleanup;
 }
 for(UINTN i=0;i<st->NumberOfTableEntries;i++) {
  EFI_CONFIGURATION_TABLE*t=&st->ConfigurationTable[i];uint64_t p=(UINTN)t->VendorTable;
  if(equal(&t->VendorGuid,&acpi1_guid,16)) h->acpi1=p;
  if(equal(&t->VendorGuid,&acpi2_guid,16)) h->acpi2=p;
  if(equal(&t->VendorGuid,&smbios2_guid,16)) h->smbios2=p;
  if(equal(&t->VendorGuid,&smbios3_guid,16)) h->smbios3=p;
 }
#if RUN_TRANSITION
 if(EFI_ERROR(status=pages((sizeof(transition_blob)+4095)/4096,EfiLoaderCode,&tr))) goto cleanup;
 #if RUN_TRANSITION == 2
 if((h->initial_pat&255)!=6 || h->framebuffer_size>0xffffffff || h->height<330) {status=EFI_UNSUPPORTED;goto cleanup;}
 if(EFI_ERROR(status=load_m3_entry(&pl,&pl_pages,&pl_entry))) goto cleanup;
 if(EFI_ERROR(status=pages(32,EfiLoaderData,&stack))) goto cleanup;
 #ifdef MILESTONE_USB
 line(L"USB arena requested bytes: ",M3_ARENA_SIZE);
 #endif
 if(EFI_ERROR(status=pages(M3_ARENA_SIZE/4096,EfiLoaderData,&arena))) {
  #ifdef MILESTONE_USB
  line(L"USB arena allocation failed: ",status);
  #endif
  goto cleanup;
 }
 #ifdef MILESTONE_USB
 if(arena<0x1000000) {status=EFI_OUT_OF_RESOURCES;goto cleanup;}
 {
  EFI_PHYSICAL_ADDRESS Low=0xffffff;
  status=bs->AllocatePages(AllocateMaxAddress,EfiLoaderData,M3_LOW_ARENA_SIZE/4096,&Low);
  if(EFI_ERROR(status)) {line(L"USB low arena allocation failed: ",status);goto cleanup;}
  low_arena=Low;
  if(Low<0x100000 || Low+M3_LOW_ARENA_SIZE>0x1000000) {status=EFI_UNSUPPORTED;goto cleanup;}
  zero((void*)(UINTN)Low,M3_LOW_ARENA_SIZE);
  m3->low_arena_base=Low;m3->low_arena_size=M3_LOW_ARENA_SIZE;
  line(L"USB low arena base: ",Low);
 }
 {
  EFI_PHYSICAL_ADDRESS Low=0x9ffff;
  status=bs->AllocatePages(AllocateMaxAddress,EfiLoaderData,M3_REALMODE_ARENA_SIZE/4096,&Low);
  if(EFI_ERROR(status)) {line(L"USB realmode arena allocation failed: ",status);goto cleanup;}
  realmode_arena=Low;
  /* Stock AllocatePages owns these pages; leave IVT/BDA and all stock
   * reserved memory alone. Never claim legacy low RAM without ownership. */
  if(Low<0x10000 || Low+M3_REALMODE_ARENA_SIZE>0xa0000) {status=EFI_UNSUPPORTED;goto cleanup;}
  zero((void*)(UINTN)Low,M3_REALMODE_ARENA_SIZE);
  m3->realmode_arena_base=Low;m3->realmode_arena_size=M3_REALMODE_ARENA_SIZE;
  line(L"USB realmode arena base: ",Low);
 }
 #endif
 if(EFI_ERROR(status=pages((sizeof(fv_blob)+4095)/4096,EfiLoaderData,&fv))) goto cleanup;
 copy((void*)(UINTN)fv,fv_blob,sizeof(fv_blob));
 m3->arena_base=arena;m3->arena_size=M3_ARENA_SIZE;
 m3->fv_base=fv;m3->fv_size=sizeof(fv_blob);
 m3->entry_base=pl;m3->entry_size=pl_pages*4096;
#else
 if(EFI_ERROR(status=pages((sizeof(payload_blob)+4095)/4096,EfiLoaderCode,&pl))) goto cleanup;
 if(EFI_ERROR(status=pages(16,EfiLoaderData,&stack))) goto cleanup;
#endif
 if(EFI_ERROR(status=pages(6,EfiLoaderData,&pt))) goto cleanup;
 copy((void*)(UINTN)tr,transition_blob,sizeof(transition_blob));
 #if RUN_TRANSITION != 2
 copy((void*)(UINTN)pl,payload_blob,sizeof(payload_blob));
#endif
 h->transition_base=tr;h->transition_size=sizeof(transition_blob);
#if RUN_TRANSITION == 2
 h->payload_base=pl;h->payload_size=pl_pages*4096;
 h->stack_base=stack;h->stack_size=128*1024;h->page_tables=pt;
#else
 h->payload_base=pl;h->payload_size=sizeof(payload_blob);
 h->stack_base=stack;h->stack_size=65536;h->page_tables=pt;
#endif
 uint64_t *pml4=(void*)(UINTN)pt,*pdpt=(void*)(UINTN)(pt+4096);
 pml4[0]=(pt+4096)|3;
 for(unsigned i=0;i<4;i++) {
  pdpt[i]=(pt+8192+i*4096)|3;
  uint64_t *pd=(void*)(UINTN)(pt+8192+i*4096);
  for(unsigned j=0;j<512;j++) {
   uint64_t base=((uint64_t)i*512+j)<<21;
   /* UC on framebuffer PDEs, preserve PAT/MTRRs. No MMIO registers are accessed. */
   unsigned attr=0x83;
   if(base<h->framebuffer+h->framebuffer_size && base+0x200000>h->framebuffer) attr|=0x18;
   pd[j]=base|attr;
  }
 }
#endif
 sz=capacity;status=bs->GetMemoryMap(&sz,(void*)(UINTN)mp,&key,&ds,&dv);
 if(EFI_ERROR(status) || ds<sizeof(EFI_MEMORY_DESCRIPTOR) || !ds || sz%ds) {status=EFI_BAD_BUFFER_SIZE;goto cleanup;}
 h->memory_map_size=sz;h->descriptor_size=ds;h->descriptor_version=dv;
 if(!acpi(h,h->acpi1)||!acpi(h,h->acpi2)||!smbios(h,h->smbios2,0)||!smbios(h,h->smbios3,1)) {
  say(L"Invalid/unmapped ACPI or SMBIOS; refused.\r\n");status=EFI_COMPROMISED_DATA;goto cleanup;
 }
 h->flags=(h->acpi1||h->acpi2?1u:0u)|(h->smbios2||h->smbios3?2u:0u);
 line(L"CPUID: ",h->cpuid_signature);line(L"Framebuffer: ",h->framebuffer);
 line(L"Width: ",h->width);line(L"Height: ",h->height);line(L"Stride: ",h->stride);
 line(L"Pixel format: ",h->pixel_format);line(L"ACPI2 RSDP: ",h->acpi2);
 line(L"ACPI1 RSDP: ",h->acpi1);line(L"ACPI tables: ",h->table_count);
 line(L"SMBIOS2: ",h->smbios2);line(L"SMBIOS3: ",h->smbios3);
 line(L"Memory map bytes: ",h->memory_map_size);line(L"Handoff: ",hp);line(L"Tables present flags (ACPI=1 SMBIOS=2): ",h->flags);
#if RUN_TRANSITION
#if RUN_TRANSITION == 2
 line(L"M3 arena base: ",arena);line(L"M3 FV base: ",fv);line(L"M3 entry point: ",pl_entry);
 say(L"M3: ExitBootServices then EDK2 X64 DXE.\r\n");
#else
 say(L"M2: ExitBootServices then long mode. No EFI calls afterwards.\r\n");
 #endif
 /* Last firmware console operation. A retry uses only GetMemoryMap/EBS. */
 for(unsigned retry=0;retry<3;retry++) {
  sz=capacity;status=bs->GetMemoryMap(&sz,(void*)(UINTN)mp,&key,&ds,&dv);
  if(EFI_ERROR(status)) {stop();}
  h->memory_map_size=sz;h->descriptor_size=ds;h->descriptor_version=dv;
  status=bs->ExitBootServices(image,key);
  if(!EFI_ERROR(status)) {
   typedef void (__attribute__((cdecl))*Enter)(uint32_t,uint32_t,uint32_t,uint32_t);
   #if RUN_TRANSITION == 2
   ((Enter)(UINTN)tr)((uint32_t)pt,(uint32_t)(stack+128*1024),(uint32_t)pl_entry,(uint32_t)hp);
#else
   ((Enter)(UINTN)tr)((uint32_t)pt,(uint32_t)(stack+65536),(uint32_t)pl,(uint32_t)hp);
#endif
   stop();
  }
  if(status!=EFI_INVALID_PARAMETER) stop();
 }
 stop();
#else
 say(L"M1 COLLECT OK. Photograph these values. Returning in 30 seconds.\r\n");
 bs->Stall(30000000);status=EFI_SUCCESS;
#endif
cleanup:
 if(EFI_ERROR(status)) {line(L"PRE-EXIT ERROR: ",status);bs->Stall(10000000);}
#if RUN_TRANSITION
 if(pt) bs->FreePages(pt,6);
 #if RUN_TRANSITION == 2
 if(fv) bs->FreePages(fv,(sizeof(fv_blob)+4095)/4096);
 if(arena) bs->FreePages(arena,M3_ARENA_SIZE/4096);
 if(realmode_arena) bs->FreePages(realmode_arena,M3_REALMODE_ARENA_SIZE/4096);
 if(low_arena) bs->FreePages(low_arena,M3_LOW_ARENA_SIZE/4096);
 if(stack) bs->FreePages(stack,32);
 if(pl) bs->FreePages(pl,pl_pages);
#else
 if(stack) bs->FreePages(stack,16);
 if(pl) bs->FreePages(pl,(sizeof(payload_blob)+4095)/4096);
#endif
 if(tr) bs->FreePages(tr,(sizeof(transition_blob)+4095)/4096);
#else
 (void)tr;(void)pl;(void)stack;(void)pt;
#endif
 if(mp) bs->FreePages(mp,16);
 if(hp) bs->FreePages(hp,1);
 return status;
}
