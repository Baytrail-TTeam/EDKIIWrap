from pathlib import Path
import shutil
root=Path(__file__).resolve().parent.parent
out=root/'build/usb/workspace/Build/BaytrailUsbPlatform/DEBUG_CLANGPDB'
parts=[]
for name,path in [('transition_blob',root/'build/usb/transition.bin'),('payload_blob',out/'X64/M3Entry.efi'),('fv_blob',out/'FV/USBDXE.Fv')]:
 data=path.read_bytes();parts.append(f'static const unsigned char {name}[]={{\n'+','.join(str(b) for b in data)+'\n};\n')
(root/'build/usb/blobs.h').write_text(''.join(parts))
for path in (out/'X64').glob('*.efi'):shutil.copyfile(path,root/'build/usb'/path.name)
shutil.copyfile(out/'FV/USBDXE.Fv',root/'build/usb/USBDXE.Fv')
