"""Deterministic FAT16 superfloppy; retain a sparse prefix for read-only RAM IO."""
from pathlib import Path
import struct,hashlib,json,argparse
root=Path(__file__).resolve().parent.parent
parser=argparse.ArgumentParser()
parser.add_argument('--app',type=Path,default=root/'build/usb/BOOTX64.EFI')
parser.add_argument('--output',type=Path,default=root/'build/usb/qemu-usb')
args=parser.parse_args()
shell=args.app.read_bytes()
assert len(shell)<(8192-76)*512,'Application does not fit the test FAT image'
sector=512;total=8192;spf=32;root_entries=128;data_sector=1+2*spf+8
image=bytearray(total*sector)
image[:3]=b'\xeb\x3c\x90';image[3:11]=b'BTX64   '
struct.pack_into('<HBHBHHBHHHII',image,11,512,1,1,2,root_entries,total,0xf8,spf,32,64,0,0)
image[36:39]=b'\x80\x00\x29';struct.pack_into('<I',image,39,0x42545835)
image[43:54]=b'BTUSBTEST  ';image[54:62]=b'FAT16   ';image[510:512]=b'\x55\xaa'
fat=bytearray(spf*512);struct.pack_into('<HH',fat,0,0xfff8,0xffff)
next_cluster=2
def allocate(data):
 global next_cluster
 first=next_cluster;count=(len(data)+511)//512
 for i in range(count):
  c=next_cluster;next_cluster+=1;struct.pack_into('<H',fat,c*2,0xffff if i+1==count else c+1)
  off=(data_sector+c-2)*512;image[off:off+len(data[i*512:(i+1)*512])]=data[i*512:(i+1)*512]
 return first
def entry(name,attr,cluster,size=0):
 b=bytearray(32);b[:11]=name.encode('ascii');b[11]=attr
 struct.pack_into('<H',b,16,0x5821);struct.pack_into('<H',b,24,0x5821) # fixed FAT date 2024-01-01
 struct.pack_into('<HI',b,26,cluster,size);return b
# Reserve directories, then contiguous file data.
efi=allocate(bytes(512));boot=allocate(bytes(512))
script=b'\xff\xfe'+('echo -off\r\nver\r\nmap -r\r\necho SHELL_X64_RAM_OK\r\n').encode('utf-16le')
app=allocate(shell);startup=allocate(script)
def directory(c,records):
 off=(data_sector+c-2)*512
 image[off:off+32*len(records)]=b''.join(records)
directory(efi,[entry('.          ',16,efi),entry('..         ',16,0),entry('BOOT       ',16,boot)])
directory(boot,[entry('.          ',16,boot),entry('..         ',16,efi),entry('BOOTX64 EFI',32,app,len(shell))])
r=(1+2*spf)*512
image[r:r+96]=entry('BTUSBTEST  ',8,0)+entry('EFI        ',16,efi)+entry('STARTUP NSH',32,startup,len(script))
image[512:512+len(fat)]=fat;image[(1+spf)*512:(1+2*spf)*512]=fat
prefix=(data_sector+next_cluster-2)*512
out=args.output;out.mkdir(parents=True,exist_ok=True)
(out/'usb-disk.raw').write_bytes(image[:prefix])
(out/'usb-disk.img').write_bytes(image)
(out/'usb-disk.json').write_text(json.dumps({'virtual_size':len(image),'prefix_size':prefix,'shell_sha256':hashlib.sha256(shell).hexdigest(),'fat16_image_sha256':hashlib.sha256(image).hexdigest()},indent=2)+'\n')
print('Shell FAT16:',len(image),'bytes, sparse prefix',prefix)
