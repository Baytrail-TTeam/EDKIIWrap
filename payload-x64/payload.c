#include "Handoff.h"
/* Deliberately no EFI pointers, library calls, floating point or global state. */
static uint32_t component(uint32_t mask, unsigned value) {
 uint32_t out=0; unsigned bit=0;
 for(unsigned i=0;i<32;i++) if(mask & (1u<<i)) {
  if(value & (1u<<bit)) out|=1u<<i;
  bit++;
 }
 return out;
}
static uint32_t color(const Handoff *h, unsigned r,unsigned g,unsigned b) {
 if(h->pixel_format==0) return r|(g<<8)|(b<<16);
 if(h->pixel_format==1) return b|(g<<8)|(r<<16);
 return component(h->red_mask,r)|component(h->green_mask,g)|component(h->blue_mask,b);
}
static void pixel(const Handoff*h,unsigned x,unsigned y,uint32_t c) {
 if(x>=h->width || y>=h->height) return;
 uint64_t offset=((uint64_t)y*h->stride+x)*4;
 if(offset+4<=h->framebuffer_size)
  *(volatile uint32_t *)(uintptr_t)(h->framebuffer+offset)=c;
}
/* 5x7 rows for precisely the message below. All addressing is RIP-relative. */
static const unsigned char glyphs[15][7]={
 {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4}, {14,17,17,31,17,17,17},
 {14,17,16,23,17,17,14}, {31,16,16,30,16,16,31}, {0,0,0,0,0,0,0},
 {14,17,1,2,4,8,31}, {0,0,0,0,0,0,0}, {17,17,10,4,10,17,17},
 {6,8,16,30,17,17,14}, {2,6,10,18,31,2,2}, {0,0,0,0,0,0,0},
 {14,17,17,17,17,17,14}, {17,18,20,24,20,18,17}, {0,0,0,0,0,0,0}
};
__attribute__((ms_abi,section(".text.entry"),noreturn))
void payload_entry(const Handoff *h) {
 if(h->magic==HANDOFF_MAGIC && h->version==1 && h->size==sizeof(*h)) {
  uint32_t bg=color(h,0,0,128),fg=color(h,255,255,255),green=color(h,0,255,0);
  for(unsigned y=8;y<64;y++) for(unsigned x=8;x<356;x++) pixel(h,x,y,bg);
  for(unsigned x=8;x<356;x++) pixel(h,x,8,green);
  for(unsigned n=0;n<14;n++) for(unsigned y=0;y<7;y++) for(unsigned x=0;x<5;x++)
   if(glyphs[n][y] & (1u<<(4-x)))
    for(unsigned dy=0;dy<3;dy++) for(unsigned dx=0;dx<3;dx++)
     pixel(h,16+n*24+x*3+dx,20+y*3+dy,fg);
 }
 for(;;) __asm__ volatile("cli; hlt");
}
