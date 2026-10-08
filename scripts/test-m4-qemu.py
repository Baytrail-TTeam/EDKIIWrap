"""Boot our IA32 EFI in IA32 OVMF; inspect framebuffer markers, stop VM."""
import hashlib,json,os,shutil,socket,struct,subprocess,tempfile,time,zlib
from pathlib import Path
root=Path(__file__).resolve().parent.parent
out=root/'build/m4/qemu-m4'
firmware=Path(os.environ.get('QEMU_FIRMWARE','/opt/homebrew/share/qemu'))
code=firmware/'edk2-i386-code.fd';vars_template=firmware/'edk2-i386-vars.fd'
assert code.exists() and vars_template.exists(), 'Set QEMU_FIRMWARE to directory containing IA32 OVMF code and vars.'
esp=out/'esp/EFI/BOOT';esp.mkdir(parents=True,exist_ok=True)
loader=root/'build/BAYTRAIL-M4-IA32.EFI'
shutil.copyfile(loader,esp/'BOOTIA32.EFI');shutil.copyfile(vars_template,out/'vars.fd')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
with tempfile.TemporaryDirectory(prefix='baytrail-m4-') as tmp:
 sock=Path(tmp)/'qmp.sock'
 command=['qemu-system-x86_64','-machine','q35,accel=tcg','-cpu','max','-m','256','-smp','1',
 '-drive',f'if=pflash,format=raw,unit=0,readonly=on,file={code}',
 '-drive',f'if=pflash,format=raw,unit=1,file={out/"vars.fd"}',
 '-drive',f'format=raw,file=fat:rw:{out/"esp"}',
 '-display','none','-serial',f'file:{out/"serial.log"}','-monitor','none',
 '-qmp',f'unix:{sock},server=on,wait=off','-net','none','-no-reboot','-no-shutdown']
 with (out/'qemu.log').open('wb') as log:
  proc=subprocess.Popen(command,stdout=log,stderr=log)
  try:
   deadline=time.monotonic()+45
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
   qmp('qmp_capabilities');counts=[]
   while time.monotonic()<deadline:
    qmp('screendump',{'filename':str(out/'screen.ppm')})
    with (out/'screen.ppm').open('rb') as p:
     assert p.readline().strip()==b'P6'
     dims=p.readline()
     while dims.startswith(b'#'):dims=p.readline()
     w,h=map(int,dims.split());assert p.readline().strip()==b'255';pixels=p.read()
    assert len(pixels)==w*h*3
    counts=[sum(pixels[i:i+3]==b'\x40\xff\x40' for i in range((8+row*32)*w*3,min(h,36+row*32)*w*3,3)) for row in (0,1,2,3,4,5)]
    if min(counts)>100:break
    time.sleep(1)
   registers=qmp('human-monitor-command',{'command-line':'info registers'})
   (out/'registers.txt').write_text(registers)
   bars=[pixels[(280*w+x)*3:(280*w+x)*3+3].hex() for x in (60,160,260)]
   success=bars==['ff0000','00ff00','0000ff'] and min(counts)>100 and 'CS64' in registers and 'HLT=1' in registers
   result={'success':success,'loader_sha256':sha(loader),'firmware_code_sha256':sha(code),
   'firmware_vars_template_sha256':sha(vars_template),'rgb_chart_pixels':bars,'green_pixels_rows_0_to_5':counts,
   'qemu_version':subprocess.check_output(['qemu-system-x86_64','--version'],text=True).splitlines()[0],
   'command':command,'date':'2026-10-08','scope':'QEMU TCG IA32 OVMF only; tablet not tested'}
   (out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
   qmp('quit');proc.wait(timeout=5)
   def chunk(kind,data):
    return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
   scanlines=b''.join(b'\0'+pixels[y*w*3:(y+1)*w*3] for y in range(h))
   png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(scanlines))+chunk(b'IEND',b'')
   (out/'screen.png').write_bytes(png)
   print(json.dumps(result,indent=2))
   assert success,'M4 markers or X64 halted state missing; inspect screen.ppm and registers.txt'
  finally:
   if proc.poll() is None:
    proc.terminate()
    try:proc.wait(timeout=5)
    except subprocess.TimeoutExpired:proc.kill();proc.wait()
