"""Read-only report of the pinned donor; never load or execute its code."""
import hashlib,json,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parent.parent
repo=root/'third_party/mrchromebox-blobs'
p=repo/'soc/intel/byt/IntelGopDriver.efi';b=p.read_bytes()
pe=struct.unpack_from('<I',b,60)[0];machine,nsec=struct.unpack_from('<HH',b,pe+4)
opt=pe+24;optsz=struct.unpack_from('<H',b,pe+20)[0]
sections=[]
for i in range(nsec):
 off=opt+optsz+i*40
 _,vsize,rva,size,raw=struct.unpack_from('<8sIIII',b,off)
 sections.append((rva,size,raw))
def rva_offset(addr):
 for rva,size,raw in sections:
  if rva<=addr<rva+size:return raw+addr-rva
 raise ValueError('unmapped RVA')
# Supported() compares vendor 8086 at RVA 3864 and four device IDs
# from RVA 33c0 at RVA 388f. Disassembly is saved in build/.
ids=struct.unpack_from('<4H',b,rva_offset(0x33c0))
vbt=(repo/'soc/intel/byt/vbt.bin').read_bytes();start=vbt.index(b'$VBT')
ver,hs,size=struct.unpack_from('<HHH',vbt,start+20)
bdb=struct.unpack_from('<I',vbt,start+28)[0];bver,bhs,bsz=struct.unpack_from('<HHH',vbt,start+bdb+16)
commit=subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD'],text=True).strip()
report={
 'source':f'https://github.com/MrChromebox/blobs/blob/{commit}/soc/intel/byt/IntelGopDriver.efi',
 'sha256':hashlib.sha256(b).hexdigest(),'size':len(b),'pe_machine':hex(machine),
 'subsystem':struct.unpack_from('<H',b,opt+68)[0],
 'identity_string':'Intel(R) Valley View Graphics Controller',
 'gop_version':'7.2.1013','version_evidence':'soc/intel/byt/versions.txt (repository metadata)',
 'supported_pci_ids':[f'8086:{i:04x}' for i in ids],
 'pci_evidence':'Supported() disassembly RVAs 0x3864, 0x3885-0x389b; table RVA 0x33c0',
 'companion_vbt':{'path':'soc/intel/byt/vbt.bin','sha256':hashlib.sha256(vbt).hexdigest(),
  'version':ver,'declared_size':size,'bdb_version':bver,'bdb_size':bsz,'checksum':sum(vbt[start:start+size])%256},
 'limits':['VBT is a separate donor artifact, not an embedded GOP VBT.',
  'Matching PCI ID and BDB version do not verify panel, PMIC or tablet compatibility.',
  'Distribution rights not established; blob stays in ignored third_party.',
  'Not executed or included in M1/M2.']}
assert machine==0x8664 and 0x0f31 in ids
(root/'docs/GOP-DONOR.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
