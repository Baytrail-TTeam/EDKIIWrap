from pathlib import Path
import shutil
root=Path(__file__).resolve().parent.parent
out=root/'build/m3/workspace/Build/BaytrailM3/DEBUG_CLANGPDB'
items=[('transition_blob',root/'build/transition.bin'),('payload_blob',out/'X64/M3Entry.efi'),('fv_blob',out/'FV/M3DXE.Fv')]
parts=[]
for name,path in items:
 data=path.read_bytes()
 parts.append(f'static const unsigned char {name}[]={{\n'+','.join(str(b) for b in data)+'\n};\n')
(root/'build/m3/blobs.h').write_text(''.join(parts))
for name in ['M3Entry.efi','DxeCore.efi','RuntimeDxe.efi','BaytrailDxeProbe.efi']:
 shutil.copyfile(out/'X64'/name,root/'build/m3'/name)
shutil.copyfile(out/'FV/M3DXE.Fv',root/'build/m3/M3DXE.Fv')
