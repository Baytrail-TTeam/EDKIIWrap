import struct, subprocess, os
from pathlib import Path
root=Path(__file__).resolve().parent.parent
lines=[]
for stage in (1,2):
 p=root/'build'/f'BAYTRAIL-M{stage}-IA32.EFI';b=p.read_bytes()
 pe=struct.unpack_from('<I',b,0x3c)[0]
 assert b[:2]==b'MZ' and b[pe:pe+4]==b'PE\0\0'
 machine,nsec,stamp=struct.unpack_from('<HHI',b,pe+4)
 opt=pe+24
 assert machine==0x14c and stamp==0
 assert struct.unpack_from('<H',b,opt)[0]==0x10b
 assert struct.unpack_from('<H',b,opt+68)[0]==10
 reloc_rva,reloc_size=struct.unpack_from('<II',b,opt+96+5*8)
 assert reloc_rva and reloc_size
 assert struct.unpack_from("<II",b,opt+96+8)==(0,0) # no PE imports
 assert struct.unpack_from('<I',b,opt+16)[0]!=0
 lines.append(f'{p.name}: PE32 IA32 (014c), EFI application (10), relocations present, timestamp=0, {len(b)} bytes')
elf=(root/'build/payload.elf').read_bytes()
assert elf[:5]==b'\x7fELF\x02' and struct.unpack_from('<H',elf,18)[0]==62
assert struct.unpack_from('<Q',elf,24)[0]==0
llvm=os.environ.get('LLVM_BIN','/opt/homebrew/opt/llvm/bin')
report=subprocess.check_output([f'{llvm}/llvm-readobj','--relocations','--symbols',str(root/'build/payload.elf')],text=True)
assert 'Relocations [\n]' in report
# No unresolved symbols (the null ELF symbol is allowed).
for symbol in report.split('  Symbol {')[1:]:
 if 'Section: Undefined' in symbol: assert 'Name:  (0)' in symbol
lines.append('payload.elf: ELF64 x86-64, entry=0, no relocations or named undefined symbols')
assert len((root/'build/transition.bin').read_bytes())<4096
lines.append('transition.bin: fits one page; NASM listing records mixed 32/64-bit code')
lines.append('STATIC CHECKS ONLY: no EFI artifact has been executed; M1/M2 runtime unverified.')
(root/'build/verification.txt').write_text('\n'.join(lines)+'\n')
print('\n'.join(lines))
