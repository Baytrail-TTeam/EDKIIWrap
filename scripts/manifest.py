import hashlib,json,os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parent.parent
files=['BAYTRAIL-M1-IA32.EFI','BAYTRAIL-M2-IA32.EFI','transition.bin','payload.bin','payload.elf']
checks={f:hashlib.sha256((root/'build'/f).read_bytes()).hexdigest() for f in files}
(root/'build/SHA256SUMS').write_text(''.join(f'{h}  {f}\n' for f,h in checks.items()))
tools={}
for label,exe in [('clang',os.environ.get('CC',os.environ.get('LLVM_BIN','/opt/homebrew/opt/llvm/bin')+'/clang')),('lld',os.environ.get('LLD_BIN','/opt/homebrew/opt/lld/bin')+'/ld.lld'),('nasm','nasm')]:
 tools[label]=subprocess.check_output([exe,'--version'],text=True).splitlines()[0]
(root/'build/manifest.json').write_text(json.dumps({'tools':tools,'sources':json.loads((root/'third_party/sources.lock.json').read_text()),'sha256':checks,'hardware_tested':False},indent=2)+'\n')
