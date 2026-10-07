"""Fetch pinned source trees into ignored third_party; no firmware execution."""
import json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parent.parent
lock=json.loads((root/'third_party/sources.lock.json').read_text())
def run(*args):subprocess.run(args,check=True)
for source in lock['sources']:
 path=root/source['path']
 if path.exists():
  head=subprocess.check_output(['git','-C',str(path),'rev-parse','HEAD'],text=True).strip()
  if head!=source['commit']:raise SystemExit(f'Refusing to reset existing checkout: {path}')
 else:
  run('git','clone','--no-checkout','--filter=blob:none',source['url'],str(path))
  run('git','-C',str(path),'fetch','--depth','1','origin',source['commit'])
  run('git','-C',str(path),'checkout','--detach',source['commit'])
edk=root/'third_party/edk2'
paths=[s['path'] for s in lock['initialized_edk2_submodules']]
run('git','-C',str(edk),'submodule','update','--init','--depth','1',*paths)
run('python3',str(root/'scripts/check-sources.py'))
