"""Boot our IA32 EFI in IA32 OVMF; inspect framebuffer markers, stop VM."""
import argparse,hashlib,json,os,re,shutil,socket,struct,subprocess,tempfile,time,zlib
from datetime import datetime
from pathlib import Path
import runpy
marker=runpy.run_path(str(Path(__file__).with_name("marker-screen.py")))["marker"]
root=Path(__file__).resolve().parent.parent
parser=argparse.ArgumentParser()
parser.add_argument('--case',choices=['direct','hub-mbr','wrong-app','no-keyboard','removable','ventoy','dma-failure','windows-iso','efi-app','cpu-fault','exit-boot-services'],default='direct')
parser.add_argument('--ventoy-app',type=Path,help='Explore this user EFI in an isolated FAT image; no automatic success verdict')
parser.add_argument('--efi-app',type=Path,default=root/'build/usb/inputs/windows-bootx64.efi',help='EFI application for isolated exploratory test')
parser.add_argument('--windows-iso',type=Path,help='Read-only Windows ISO USB CD; isolated driver disk, no physical disk access')
parser.add_argument('--iso-driver',type=Path,default=root/'build/usb/inputs/iso9660_x64.efi',help='Copy of ISO9660 X64 driver used for direct ISO test')
parser.add_argument('--native-reference',action='store_true',help='Compare Windows ISO in stock X64 OVMF, without our loader')
parser.add_argument('--loader',type=Path,default=root/'build/BAYTRAIL-USB-IA32.EFI',help='Loader candidate for controlled before/after comparison')
parser.add_argument('--memory-mib',type=int,default=1536,help='QEMU physical RAM; USB loader reserves 1024 MiB')
parser.add_argument('--observe-seconds',type=int,help='Observe exploratory application for this many seconds')
parser.add_argument('--kernel-image',type=Path,help='Exact extracted ntoskrnl.exe for kernel address and bugcheck inspection')
parser.add_argument('--setup-keyboard-test',action='store_true',help='At observed Windows Setup send Tab to move focus; review final screenshot')
parser.add_argument('--capture-handoff',action='store_true',help='Inspect our handoff and ACPI in temporary QEMU physical RAM dump')
parser.add_argument('--kernel-trace',action='store_true',help='QEMU translated kernel blocks at high virtual addresses')
args=parser.parse_args();case=args.case
assert args.observe_seconds is None or 1<=args.observe_seconds<=600
assert 128<=args.memory_mib<=4096
assert not args.native_reference or case=='windows-iso'
assert not args.setup_keyboard_test or case=='windows-iso'
exploratory=case in ['ventoy','windows-iso','efi-app']
out=root/('build/usb/qemu-usb' if case=='direct' else 'build/usb/qemu-'+case)
if args.native_reference:out=root/'build/usb/qemu-windows-reference'
out.mkdir(parents=True,exist_ok=True)
if case=='efi-app':
 assert args.efi_app.is_file()
 subprocess.check_call(['python3',str(root/'scripts/make-usb-test-disk.py'),'--app',str(args.efi_app),'--output',str(out)])
if case=='ventoy':
 assert args.ventoy_app and args.ventoy_app.is_file(),'--ventoy-app is required'
 subprocess.check_call(['python3',str(root/'scripts/make-usb-test-disk.py'),'--app',str(args.ventoy_app),'--output',str(out)])
if case=='windows-iso':
 assert args.windows_iso and args.windows_iso.is_file(), '--windows-iso is required'
 assert args.iso_driver.is_file(),'--iso-driver is required for windows-iso'
 subprocess.check_call(['python3',str(root/'scripts/make-usb-test-disk.py'),'--app',str(args.iso_driver),'--output',str(out)])
if case not in ['direct','ventoy','windows-iso','efi-app']:
 data=bytearray((root/'build/usb/qemu-usb/usb-disk.img').read_bytes())
 if case=='wrong-app':
  off=data.find((root/'build/usb/BOOTX64.EFI').read_bytes());assert off>0;data[off]^=1
 if case=='hub-mbr':
  struct.pack_into('<I',data,28,2048)
  mbr=bytearray(2048*512)
  mbr[446:462]=bytes([0,0xfe,0xff,0xff,6,0xfe,0xff,0xff])+struct.pack('<II',2048,len(data)//512)
  mbr[510:512]=b'\x55\xaa';data=mbr+data
 (out/'usb-disk.img').write_bytes(data)
negative=case in ['no-keyboard','dma-failure','cpu-fault']
manual_app=case not in ['wrong-app','no-keyboard','dma-failure','cpu-fault','exit-boot-services']
firmware=Path(os.environ.get('QEMU_FIRMWARE','/opt/homebrew/share/qemu'))
code=firmware/('edk2-x86_64-code.fd' if args.native_reference else 'edk2-i386-code.fd');vars_template=firmware/'edk2-i386-vars.fd'
assert code.exists() and vars_template.exists(), 'Set QEMU_FIRMWARE to directory containing IA32 OVMF code and vars.'
esp=out/'esp/EFI/BOOT';esp.mkdir(parents=True,exist_ok=True)
loader=args.loader
shutil.copyfile(loader,esp/'BOOTIA32.EFI');shutil.copyfile(vars_template,out/'vars.fd')
def sha(p):
 digest=hashlib.sha256()
 with p.open('rb') as stream:
  for chunk in iter(lambda:stream.read(1048576),b''):digest.update(chunk)
 return digest.hexdigest()
def inspect_kernel(path,registers,qmp):
 data=path.read_bytes();pe=struct.unpack_from('<I',data,60)[0];count=struct.unpack_from('<H',data,pe+6)[0]
 sections=pe+24+struct.unpack_from('<H',data,pe+20)[0]
 def rva(offset):
  for i in range(count):
   va,size,raw=struct.unpack_from('<III',data,sections+i*40+12)
   if raw<=offset<raw+size:return va+offset-raw
 rip_match=re.search(r'RIP=([0-9a-f]+)',registers,re.I)
 if not rip_match:return {'error':'RIP unavailable'}
 rip=int(rip_match.group(1),16);start=rip-64
 raw=qmp('human-monitor-command',{'command-line':f'x/128bx 0x{start:x}'})
 values=[]
 for line in raw.splitlines():
  if ':' in line:values.extend(int(x,16) for x in re.findall(r'0x([0-9a-f]{2})\b',line.split(':',1)[1],re.I))
 memory=bytes(values);votes={}
 for offset in range(0,len(memory)-31,8):
  pattern=memory[offset:offset+32];found=data.find(pattern)
  if found<0 or data.find(pattern,found+1)>=0:continue
  address=rva(found)
  if address is None:continue
  base=start+offset-address
  if base&4095:continue
  votes[base]=votes.get(base,0)+1
 if not votes or max(votes.values())<2:return {'matched':False,'rip':hex(rip),'memory_bytes':raw}
 base=max(votes,key=votes.get)
 readobj=shutil.which('llvm-readobj') or '/opt/homebrew/opt/llvm/bin/llvm-readobj'
 exports=subprocess.check_output([readobj,'--coff-exports',str(path)],text=True)
 match=re.search(r'Name: KiBugCheckData\n  RVA: (0x[0-9A-Fa-f]+)',exports)
 result={'matched':True,'kernel_sha256':sha(path),'loaded_base':hex(base),'rip':hex(rip),'rip_rva':hex(rip-base),'signature_matches':votes[base]}
 if match:
  address=base+int(match.group(1),16)
  dump=qmp('human-monitor-command',{'command-line':f'x/5gx 0x{address:x}'})
  words=[]
  for line in dump.splitlines():
   if ':' in line:words.extend(int(x,16) for x in re.findall(r'0x([0-9a-f]+)',line.split(':',1)[1],re.I))
  result.update({'bugcheck_data_address':hex(address),'bugcheck_words':[hex(x) for x in words],'bugcheck_memory':dump})
 return result
def inspect_handoff(out,memory_mib,qmp):
 import mmap
 dump=out/'physical-ram.bin';qmp('stop')
 qmp('human-monitor-command',{'command-line':f'pmemsave 0 {memory_mib*1048576} "{dump}"'})
 result={'candidates':[]}
 with dump.open('rb') as file:
  with mmap.mmap(file.fileno(),0,access=mmap.ACCESS_READ) as ram:
   start=0;magic=bytes.fromhex('4259544c484f4631')
   while True:
    at=ram.find(magic,start)
    if at<0:break
    start=at+8
    if at+2312>len(ram) or struct.unpack_from('<II',ram,at+8)!=(1,2312):continue
    count=struct.unpack_from('<I',ram,at+44)[0]
    if count>128:continue
    a1,a2=struct.unpack_from('<QQ',ram,at+96)
    entry={'physical_address':hex(at),'acpi1':hex(a1),'acpi2':hex(a2),'tables':[],'roots':[]}
    if at+2400<=len(ram) and struct.unpack_from('<Q',ram,at+2312)[0]==0x33464f484c545942:
     version,size=struct.unpack_from('<II',ram,at+2320)
     entry.update({'m3_version':version,'m3_size':size,'arena_base':hex(struct.unpack_from('<Q',ram,at+2328)[0]),'arena_size':struct.unpack_from('<Q',ram,at+2336)[0]})
     if version==2 and size==2400:entry.update({'low_arena_base':hex(struct.unpack_from('<Q',ram,at+2384)[0]),'low_arena_size':struct.unpack_from('<Q',ram,at+2392)[0]})
    for i in range(count):
     address,length,signature=struct.unpack_from('<QII',ram,at+264+i*16)
     name=signature.to_bytes(4,'little').decode('ascii',errors='replace')
     record={'address':hex(address),'length':length,'signature':name}
     if name=='FACS':record['checksum_applicable']=False
     elif address+length<=len(ram):record['checksum_ok']=sum(ram[address:address+length])%256==0
     entry['tables'].append(record)
    for ptr in set([a1,a2]):
     if not ptr or ptr+36>len(ram):continue
     root={'rsdp_address':hex(ptr),'signature':ram[ptr:ptr+8].decode('ascii',errors='replace'),'revision':ram[ptr+15],'checksum20_ok':sum(ram[ptr:ptr+20])%256==0,'trees':[]}
     rsdt=struct.unpack_from('<I',ram,ptr+16)[0];xsdt=struct.unpack_from('<Q',ram,ptr+24)[0] if ram[ptr+15]>=2 else 0
     for address,step in [(rsdt,4),(xsdt,8)]:
      if not address or address+36>len(ram):continue
      length=struct.unpack_from('<I',ram,address+4)[0]
      if length<36 or length>1048576 or address+length>len(ram):continue
      tree={'address':hex(address),'signature':ram[address:address+4].decode('ascii',errors='replace'),'checksum_ok':sum(ram[address:address+length])%256==0,'children':[]}
      for pos in range(address+36,address+length-step+1,step):
       child=struct.unpack_from('<Q' if step==8 else '<I',ram,pos)[0]
       if child+36<=len(ram):
        size=struct.unpack_from('<I',ram,child+4)[0]
        node={'address':hex(child),'signature':ram[child:child+4].decode('ascii',errors='replace'),'length':size}
        if 36<=size<=1048576 and child+size<=len(ram):
         data=ram[child:child+size];node['checksum_ok']=sum(data)%256==0
         if node['signature']=='APIC':node['raw_hex']=data.hex()
        tree['children'].append(node)
      root['trees'].append(tree)
     entry['roots'].append(root)
    result['candidates'].append(entry)
 dump.unlink();return result
usb_before=sha(out/'usb-disk.img')
with tempfile.TemporaryDirectory(prefix='baytrail-usb-') as tmp:
 sock=Path(tmp)/'qmp.sock'
 command=['qemu-system-x86_64','-machine','q35,accel=tcg','-cpu','max','-m',str(args.memory_mib),'-smp','1',
 '-drive',f'if=pflash,format=raw,unit=0,readonly=on,file={code}',
 '-drive',f'if=pflash,format=raw,unit=1,file={out/"vars.fd"}',
 '-drive',f'format=raw,file=fat:rw:{out/"esp"}',
 '-device','qemu-xhci,id=xhci',
 '-drive',f'if=none,id=usbtest,format=raw,file={out/"usb-disk.img"}',
 '-device','usb-storage,bus=xhci.0,drive=usbtest'+(',port=1.1' if case=='hub-mbr' else ',removable=on' if case=='removable' else ''),
 '-display','none','-serial',f'file:{out/"serial.log"}','-monitor','none',
 '-qmp',f'unix:{sock},server=on,wait=off','-net','none','-no-reboot','-no-shutdown']
 if args.kernel_trace:command+=['-d','in_asm','-dfilter','0xffff800000000000..0xffffffffffffffff','-D',str(out/'kernel-trace.log')]
 if case=='windows-iso':
  i=command.index(f'if=none,id=usbtest,format=raw,file={out/"usb-disk.img"}');command[i]+=',readonly=on'
  command[command.index('-display'):command.index('-display')]=['-drive',f'if=none,id=windowsiso,format=raw,media=cdrom,readonly=on,file={args.windows_iso}','-device','usb-storage,bus=xhci.0,drive=windowsiso']
 if case=='hub-mbr':command[command.index('usb-storage,bus=xhci.0,drive=usbtest,port=1.1')-1:command.index('usb-storage,bus=xhci.0,drive=usbtest,port=1.1')-1]=['-device','usb-hub,id=hub,bus=xhci.0,port=1']
 if case!='no-keyboard':command[command.index('-display'):command.index('-display')]=['-device','usb-kbd,bus=xhci.0'+(',port=1.2' if case=='hub-mbr' else '')]
 if args.native_reference:
  for flag,value in [('-drive',f'format=raw,file=fat:rw:{out/"esp"}'),('-drive',f'if=none,id=usbtest,format=raw,file={out/"usb-disk.img"},readonly=on'),('-device','usb-storage,bus=xhci.0,drive=usbtest')]:
   i=command.index(value);assert command[i-1]==flag;del command[i-1:i+1]
  command[command.index('-m')+1]='1024'
 with (out/'qemu.log').open('wb') as log:
  proc=subprocess.Popen(command,stdout=log,stderr=log)
  try:
   boot_started=time.monotonic();deadline=boot_started+max(90,(args.observe_seconds or 35)+60)
   while not sock.exists():
    if proc.poll() is not None:raise RuntimeError('QEMU exited; see qemu.log')
    if time.monotonic()>deadline:raise TimeoutError('QMP socket timeout')
    time.sleep(.1)
   s=socket.socket(socket.AF_UNIX);s.settimeout(3);s.connect(str(sock));f=s.makefile('rwb');json.loads(f.readline())
   def qmp(name,args=None):
    f.write((json.dumps({'execute':name,'arguments':args or {}})+'\n').encode());f.flush()
    while True:
     reply=json.loads(f.readline())
     if 'error' in reply:raise RuntimeError(reply)
     if 'return' in reply:return reply['return']
   qmp('qmp_capabilities');counts=[];typed=False;shell_ready_seconds=None;manual_pixels=0;app_started=None
   while time.monotonic()<deadline:
    qmp('screendump',{'filename':str(out/'screen.ppm')})
    with (out/'screen.ppm').open('rb') as p:
     assert p.readline().strip()==b'P6'
     dims=p.readline()
     while dims.startswith(b'#'):dims=p.readline()
     w,h=map(int,dims.split());assert p.readline().strip()==b'255';pixels=p.read()
    assert len(pixels)==w*h*3
    counts=[sum(pixels[i:i+3]==b'\x40\xff\x40' for i in range((8+row*32)*w*3,min(h,36+row*32)*w*3,3)) for row in (21,22,23,)]
    red=sum(pixels[i:i+3]==b'\xff\x40\x40' for i in range((8+7*32)*w*3,min(h,36+7*32)*w*3,3))
    if (negative or case=='exit-boot-services') and red>100:break
    manual_pixels=sum(pixels[i:i+3]==b'\x40\xff\x40' for i in range((8+20*32)*w*3,min(h,36+20*32)*w*3,3))
    if (counts[2]>100 if not args.native_reference else time.monotonic()-boot_started>20) and not typed:
     shell_ready_seconds=round(time.monotonic()-boot_started,3)
     assert manual_pixels==0,'USB app ran before a manual Shell command'
     command_text=('fs1:\ncd efi\ncd boot\nbootx64.efi\n' if manual_app else '')+('' if exploratory else 'echo usb_key_ok\n')
     if case=='windows-iso':command_text='load fs1:\\efi\\boot\\bootx64.efi\nmap -r\nfs2:\\efi\\boot\\bootx64.efi\n\n'
     if args.native_reference:command_text='fs0:\\efi\\boot\\bootx64.efi\n\n'
     if case=='dma-failure':command_text='fs1:\ncd efi\ncd boot\nbootx64.efi dma-map-failure\n'
     if case=='exit-boot-services':command_text='fs1:\ncd efi\ncd boot\nbootx64.efi exit-boot-services\n'
     if case=='cpu-fault':command_text='fs1:\ncd efi\ncd boot\nbootx64.efi cpu-fault\n'
     for char in command_text:
      key='spc' if char==' ' else 'ret' if char=='\n' else 'shift-minus' if char=='_' else 'shift-semicolon' if char==':' else 'dot' if char=='.' else 'minus' if char=='-' else 'backslash' if char=='\\' else char
      qmp('human-monitor-command',{'command-line':f'sendkey {key} 20'});time.sleep(.08)
      if case=='windows-iso' and char=='\n':time.sleep(2)
     typed=True;app_started=time.monotonic()
     if case=='ventoy':
      # Let GRUB finish initialization before testing direct EFI input polling.
      time.sleep(4)
      for char in 'echo ventoy_key_ok\n':
       key='spc' if char==' ' else 'ret' if char=='\n' else 'shift-minus' if char=='_' else char
       qmp('human-monitor-command',{'command-line':f'sendkey {key} 20'});time.sleep(.12)
    if case=='exit-boot-services' and manual_pixels>100:break
    if exploratory and app_started and time.monotonic()-app_started>(args.observe_seconds or (35 if case=='windows-iso' else 8)):break
    if not exploratory and min(counts)>100 and (manual_pixels>100 or not manual_app):break
    time.sleep(1)
   fault_lines={str(row):marker(pixels,w,row,b'\xff\x40\x40') for row in range(7,23)}
   fault_detail=''
   m=re.search(r'RSP +([0-9A-F]+)',fault_lines['11'])
   if m:
    fault_detail=qmp('human-monitor-command',{'command-line':'x/32gx 0x'+m.group(1)})
    (out/'fault-stack.txt').write_text(fault_detail)
   registers=qmp('human-monitor-command',{'command-line':'info registers'})
   (out/'registers.txt').write_text(registers)
   (out/'execution.txt').write_text(qmp('human-monitor-command',{'command-line':'x/64i $rip-64'})+'\n'+qmp('human-monitor-command',{'command-line':'x/64gx $rsp'}))
   handoff_result={}
   if args.capture_handoff:
    handoff_result=inspect_handoff(out,int(command[command.index('-m')+1]),qmp)
    (out/'handoff.json').write_text(json.dumps(handoff_result,indent=2)+'\n')
   kernel_result={}
   if args.kernel_image:
    kernel_result=inspect_kernel(args.kernel_image,registers,qmp)
    (out/'kernel.json').write_text(json.dumps(kernel_result,indent=2)+'\n')
   messages={str(row):marker(pixels,w,row,b'\xff\x40\x40' if row==7 else b'\x40\xff\x40') for row in [7,20,21,22,23]}
   expected={'no-keyboard':'USB KEYBOARD MISSING','dma-failure':'USB DMA MAP FAILED'}
   exact=('PAGE FAULT' in messages['7'] and '0000000100000000' in fault_lines['16']) if case=='cpu-fault' else messages['7']==expected[case] if negative else all(messages[str(row)]==text for row,text in [(21,'USB FILESYSTEM READY'),(22,'USB KEYBOARD COMMAND OK'),(23,'USB SHELL KEYBOARD READY')])
   exact=exact and (messages['20']=='USB MANUAL X64 APP OK' if manual_app else messages['20']=='')
   success=exact and (red>100 and ('HLT=1' in registers or case=='cpu-fault') if negative else min(counts)>100) and 'CS64' in registers
   if case=='exit-boot-services':success=messages['20']=='USB EXIT BOOT SERVICES OK' and 'CS64' in registers and 'HLT=1' in registers
   result={'success':None if exploratory else success,'observation_requires_review':exploratory,'visible_markers':messages,'exception_lines':fault_lines,'fault_stack':fault_detail,'case':case,'native_reference':args.native_reference,'physical_memory_mib':int(command[command.index('-m')+1]),'observe_seconds':args.observe_seconds,'kernel':kernel_result,'handoff':handoff_result,'expected_rejection':negative,'red_pixels_row_7':red,'loader_sha256':sha(loader),'firmware_code_sha256':sha(code),
   'seconds_to_shell_ready':shell_ready_seconds,'efi_app_sha256':sha(args.efi_app) if case=='efi-app' else None,'iso_driver_sha256':sha(args.iso_driver) if case=='windows-iso' else None,'windows_iso_path':str(args.windows_iso) if args.windows_iso else None,'windows_iso_size':args.windows_iso.stat().st_size if args.windows_iso else None,'ventoy_app_sha256':sha(args.ventoy_app) if case=='ventoy' else None,'manual_usb_app_requested':manual_app,'usb_disk_unchanged':usb_before==sha(out/'usb-disk.img'),'usb_disk_sha256':sha(out/'usb-disk.img'),'firmware_vars_template_sha256':sha(vars_template),'green_pixels_rows_21_22_23':counts,
   'qemu_version':subprocess.check_output(['qemu-system-x86_64','--version'],text=True).splitlines()[0],
   'command':command,'date':datetime.now().astimezone().date().isoformat(),'scope':('Successful ExitBootServices, BootServices NULL and interrupts disabled; VM halted without launching OS' if case=='exit-boot-services' else 'Isolated supplied EFI without companion files; review output' if case=='efi-app' else 'Direct read-only Windows ISO via supplied ISO9660 driver' if case=='windows-iso' else 'Exploratory user Ventoy EFI on isolated FAT; review screenshot, not a full Ventoy partition' if case=='ventoy' else 'Expected rejection of '+case if negative else 'Manual USB application from interactive Shell + keyboard command' if manual_app else 'Shell accepts USB filesystem with nonmatching BOOTX64; no automatic app launch')+'; cooperative PM timer; QEMU only; tablet not tested'}
   if args.setup_keyboard_test:
    result['setup_keyboard_input']=['tab']
    for key in result['setup_keyboard_input']:
     qmp('human-monitor-command',{'command-line':f'sendkey {key} 20'});time.sleep(.2)
    time.sleep(2);qmp('screendump',{'filename':str(out/'screen.ppm')})
    with (out/'screen.ppm').open('rb') as p:
     assert p.readline().strip()==b'P6'
     dims=p.readline()
     while dims.startswith(b'#'):dims=p.readline()
     w,h=map(int,dims.split());assert p.readline().strip()==b'255';pixels=p.read()
    assert len(pixels)==w*h*3
   (out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
   qmp('quit');proc.wait(timeout=5)
   def chunk(kind,data):
    return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
   scanlines=b''.join(b'\0'+pixels[y*w*3:(y+1)*w*3] for y in range(h))
   png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(scanlines))+chunk(b'IEND',b'')
   (out/'screen.png').write_bytes(png)
   print(json.dumps(result,indent=2))
   assert result['usb_disk_unchanged'],'USB disk was modified'
   if not exploratory:assert success,'USB storage/keyboard/Shell markers or X64 state missing; inspect screen.ppm and registers.txt'
  finally:
   if proc.poll() is None:
    proc.terminate()
    try:proc.wait(timeout=5)
    except subprocess.TimeoutExpired:proc.kill();proc.wait()
