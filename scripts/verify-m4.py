"""Verify self-contained loader, relocatable X64 images and uncompressed FV."""
import hashlib,json,struct,subprocess,os
from pathlib import Path
root=Path(__file__).resolve().parent.parent
out=root/'build/m4'
def pe(data,machine,subsystem):
 assert data[:2]==b'MZ'
 p=struct.unpack_from('<I',data,60)[0]
 assert data[p:p+4]==b'PE\0\0'
 m,n,t=struct.unpack_from('<HHI',data,p+4);o=p+24
 assert m==machine and t==0
 assert struct.unpack_from('<H',data,o)[0]==(0x10b if m==0x14c else 0x20b)
 assert struct.unpack_from('<H',data,o+68)[0]==subsystem
 dd=o+(96 if m==0x14c else 112)
 assert struct.unpack_from('<II',data,dd+8)==(0,0)
 assert not (struct.unpack_from('<H',data,p+22)[0]&1) # relocations not stripped; PIC images may need none
 reloc=struct.unpack_from('<II',data,dd+40)
 assert bool(reloc[0])==bool(reloc[1])
 entry=struct.unpack_from('<I',data,o+16)[0]
 assert 0<entry<struct.unpack_from('<I',data,o+56)[0]
 return {'machine':hex(m),'subsystem':subsystem,'entry_rva':hex(entry),'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}
files={'BAYTRAIL-M4-IA32.EFI':(root/'build/BAYTRAIL-M4-IA32.EFI',0x14c,10),
'M3Entry.efi':(out/'M3Entry.efi',0x8664,11),'DxeCore.efi':(out/'DxeCore.efi',0x8664,11),
'RuntimeDxe.efi':(out/'RuntimeDxe.efi',0x8664,12),'BaytrailFramebufferGop.efi':(out/'BaytrailFramebufferGop.efi',0x8664,11),'BaytrailM4Probe.efi':(out/'BaytrailM4Probe.efi',0x8664,11)}
results={name:pe(p.read_bytes(),m,s) for name,(p,m,s) in files.items()}
fv=(out/'M4DXE.Fv').read_bytes()
assert fv[40:44]==b'_FVH' and struct.unpack_from('<Q',fv,32)[0]==len(fv)==1048576
hl=struct.unpack_from('<H',fv,48)[0]
assert not sum(struct.unpack_from('<'+'H'*(hl//2),fv))%65536
pos=hl;images=[];types=[]
while pos+24<len(fv):
 pos=(pos+7)&~7
 if fv[pos:pos+24]==b'\xff'*24:break
 length=int.from_bytes(fv[pos+20:pos+23],'little');typ=fv[pos+18]
 assert 24<=length<=len(fv)-pos
 if typ==0xf0: # FV padding file carries extension-header bytes, no sections
  pos+=length;continue
 types.append(typ);sec=pos+24;end=pos+length
 while sec+4<=end:
  size=int.from_bytes(fv[sec:sec+3],'little');st=fv[sec+3]
  assert 4<=size<=end-sec
  if st==0x10:
   data=fv[sec+4:sec+size];pe(data,0x8664,12 if data==files['RuntimeDxe.efi'][0].read_bytes() else 11);images.append(data)
  sec=(sec+size+3)&~3
 pos+=length
assert sorted(types)==[2,5,7,7,7] # APRIORI, DXE_CORE, three drivers
for name in ('DxeCore.efi','RuntimeDxe.efi','BaytrailFramebufferGop.efi','BaytrailM4Probe.efi'):
 assert files[name][0].read_bytes() in images
loader=files['BAYTRAIL-M4-IA32.EFI'][0].read_bytes()
assert fv in loader and files['M3Entry.efi'][0].read_bytes() in loader
assert (root/'build/transition.bin').read_bytes() in loader
results['M4DXE.Fv']={'bytes':len(fv),'sha256':hashlib.sha256(fv).hexdigest(),'ffs_types':types,'pe32_sections':len(images)}
versions={}
for name,cmd in [('clang',[os.environ.get('LLVM_BIN','/opt/homebrew/opt/llvm/bin')+'/clang','--version']),('lld',[os.environ.get('LLD_BIN','/opt/homebrew/opt/lld/bin')+'/lld-link','--version']),('nasm',['nasm','-v']),('python',['python3','--version'])]:
 versions[name]=subprocess.check_output(cmd,text=True).splitlines()[0]
(out/'manifest.json').write_text(json.dumps({'static_checks_passed':True,'source_date_epoch':1791331200,'source_lock':json.loads((root/'third_party/sources.lock.json').read_text()),'toolchain':versions,'artifacts':results},indent=2)+'\n')
print('M4 static checks passed: IA32 loader, five relocatable X64 images, FV checksum/FFS/PE sections, embedded entry/FV/transition.')
