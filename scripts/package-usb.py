"""Deterministic two-file tablet test package; never accesses a mounted device."""
from pathlib import Path
import hashlib,json,zipfile
root=Path(__file__).resolve().parent.parent
out=root/'build/usb'
paths={'EFI/TOOLS/BAYTRAIL-USB-IA32.EFI':root/'build/BAYTRAIL-USB-IA32.EFI',
       'EFI/BOOT/BOOTX64.EFI':out/'BOOTX64.EFI'}
readme='''Test EDK2 X64 USB — 2026-10-08

Wypakuj katalog EFI na testowy pendrive FAT32. Zachowaj wcześniej istniejący
BOOTX64.EFI poza ścieżką testu. Nie zastępuj BOOTIA32.EFI ani plików systemu.
Podłącz pendrive i klawiaturę USB przed uruchomieniem.
Uruchom ręcznie EFI/TOOLS/BAYTRAIL-USB-IA32.EFI przez Boot From File
lub dotychczasową powłokę IA32. Poczekaj na zakończenie enumeracji (15 s).

Oczekiwane: USB STORAGE X64 APP OK, konsola Shell>,
USB SHELL KEYBOARD READY. Wpisz w powłoce:
  echo usb_key_ok
Po Enter oczekuj USB KEYBOARD COMMAND OK. Zrób zdjęcie całego ekranu.
FS0/FS1 mogą mieć różne numery; pendrive poznasz po /USB(...) w mapowaniu.

BOOTX64.EFI jest dołączoną małą aplikacją testową X64, nie powłoką ani
instalatorem Windows. Loader uruchamia z USB tylko dokładnie ten plik.
Powłoka Shell jest osadzona w RAM. USB BlockIo.WriteBlocks jest blokowane.
To PoC z pollingiem i IRQ wyłączonymi; wynik QEMU nie potwierdza jeszcze
fizycznego kontrolera Intel. Dokumentacja: docs/M5-USB.md w repo projektu.

W razie czerwonego komunikatu/zatrzymania zrób zdjęcie ekranu.
Nie wykonuj dalszych etapów Windows w tym teście.
'''
(out/'TEST-USB.txt').write_text(readme)
archive=out/'BAYTRAIL-USB-TEST.zip'
with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
 for name,path in [*paths.items(),('TEST-USB.txt',out/'TEST-USB.txt')]:
  info=zipfile.ZipInfo(name,(2026,10,8,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED;info.external_attr=0o644<<16
  z.writestr(info,path.read_bytes())
manifest={'files':{name:hashlib.sha256(path.read_bytes()).hexdigest() for name,path in paths.items()},'archive_sha256':hashlib.sha256(archive.read_bytes()).hexdigest()}
(out/'package.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Tablet package:',archive)
