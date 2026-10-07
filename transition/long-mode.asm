; Original implementation; sequence follows Intel SDM vol.3A ch.9
; and edk2 BaseLib/Ia32/EnablePaging64.nasm (see docs/RESEARCH.md).
; cdecl: enter(cr3, stack_top, payload_entry, handoff). Never returns.
BITS 32
ORG 0
start:
 cli
 cld
 pushfd
 pop eax
 and eax,0xfffbbeff             ; clear TF, NT, AC
 push eax
 popfd
 mov edx,[esp+4]
 mov esi,[esp+8]
 mov edi,[esp+12]
 mov ebx,[esp+16]
 call base
base:
 pop ebp
 sub ebp,base
 lea eax,[ebp+gdt]
 mov [ebp+gdtr+2],eax
 lgdt [ebp+gdtr]
 lea eax,[ebp+flat32]
 push dword 8
 push eax
 retf
flat32:
 mov ax,16
 mov ds,ax
 mov es,ax
 mov ss,ax
 mov fs,ax
 mov gs,ax
 ; Build fault-only IDTs; exception handlers deliberately halt.
 lea eax,[ebp+fault32]
 lea ecx,[ebp+idt32]
 mov [ebp+idtr32+2],ecx
 push edx
 mov edx,32
make32:
 mov [ecx],ax
 mov word [ecx+2],8
 mov word [ecx+4],0x8e00
 ror eax,16
 mov [ecx+6],ax
 rol eax,16
 add ecx,8
 dec edx
 jnz make32
 lidt [ebp+idtr32]
 lea eax,[ebp+fault64]
 lea ecx,[ebp+idt64]
 mov [ebp+idtr64+2],ecx
 mov edx,32
make64:
 mov [ecx],ax
 mov word [ecx+2],24
 mov word [ecx+4],0x8e00
 ror eax,16
 mov [ecx+6],ax
 rol eax,16
 mov dword [ecx+8],0
 mov dword [ecx+12],0
 add ecx,16
 dec edx
 jnz make64
 pop edx
 lea eax,[ebp+code64]
 mov [ebp+far64],eax
 mov eax,cr0
 and eax,0x7fffffff
 mov cr0,eax                    ; PG=0 (flat identity-mapped IA32 UEFI)
 mov eax,cr4
 or eax,1<<5                    ; PAE
 mov cr4,eax
 mov cr3,edx                    ; aligned PML4 physical address
 mov ecx,0xc0000080
 rdmsr
 or eax,1<<8                    ; EFER.LME, preserve remaining bits
 wrmsr
 mov eax,cr0
 or eax,0x80000001              ; PG|PE
 mov cr0,eax
 jmp far [ebp+far64]
fault32:
 cli
 hlt
 jmp fault32
BITS 64
code64:
 mov ebp,ebp                    ; explicitly zero undefined upper RBP
 lidt [rbp+idtr64]
 mov eax,16
 mov ds,ax
 mov es,ax
 mov ss,ax
 mov esp,esi                    ; zero-extend stack address
 and rsp,-16
 sub rsp,32                     ; MS x64 shadow space, aligned before call
 mov ecx,ebx                    ; sole argument: Handoff*
 mov eax,edi
 call rax
fault64:
 cli
 hlt
 jmp fault64
ALIGN 8
gdt:
 dq 0
 dq 0x00cf9a000000ffff          ; selector 8: flat code32
 dq 0x00cf92000000ffff          ; selector 16: flat data
 dq 0x00af9a000000ffff          ; selector 24: code64 L=1 D=0
gdt_end:
gdtr: dw gdt_end-gdt-1
 dd 0
far64: dd 0
 dw 24
idtr32: dw 32*8-1
 dd 0
idtr64: dw 32*16-1
 dq 0
ALIGN 16
idt32: times 32*8 db 0
idt64: times 32*16 db 0
