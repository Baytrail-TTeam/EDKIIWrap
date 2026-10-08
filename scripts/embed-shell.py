from pathlib import Path
import shutil
root=Path(__file__).resolve().parent.parent
out=root/'build/m5/workspace/Build/BaytrailShellPlatform/DEBUG_CLANGPDB'
items=[('transition_blob',root/'build/transition.bin'),('payload_blob',out/'X64/M3Entry.efi'),('fv_blob',out/'FV/SHELLDXE.Fv')]
parts=[]
for name,path in items:
 data=path.read_bytes()
 parts.append(f'static const unsigned char {name}[]={{\n'+','.join(str(b) for b in data)+'\n};\n')
(root/'build/m5/blobs.h').write_text(''.join(parts))
for name in ['M3Entry.efi','DxeCore.efi','RuntimeDxe.efi','BaytrailFramebufferGop.efi','VariableRuntimeDxe.efi','HiiDatabase.efi','GraphicsConsoleDxe.efi','EnglishDxe.efi','DiskIoDxe.efi','Fat.efi','BaytrailShellBoot.efi']:
 shutil.copyfile(out/'X64'/name,root/'build/m5'/name)
shutil.copyfile(out/'FV/SHELLDXE.Fv',root/'build/m5/SHELLDXE.Fv')

shutil.copyfile(root/'build/m5/workspace/Build/BaytrailShell/DEBUG_CLANGPDB/X64/Shell.efi',root/'build/m5/Shell.efi')
