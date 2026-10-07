import json, subprocess
from pathlib import Path
root=Path(__file__).resolve().parent.parent
lock=json.loads((root/'third_party/sources.lock.json').read_text())
for item in lock['sources']:
 path=root/item['path']
 actual=subprocess.check_output(['git','-C',str(path),'rev-parse','HEAD'],text=True).strip()
 if actual!=item['commit']: raise SystemExit(f'Source revision mismatch: {path}')

for item in lock["initialized_edk2_submodules"]:
 path=root/"third_party/edk2"/item["path"]
 actual=subprocess.check_output(["git","-C",str(path),"rev-parse","HEAD"],text=True).strip()
 if actual!=item["commit"]: raise SystemExit(f"Submodule revision mismatch: {path}")

print('Source and initialized submodule revisions match lock.')
