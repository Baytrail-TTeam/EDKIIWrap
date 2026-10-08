"""Deterministic two-file tablet test package; never accesses a mounted device."""
from pathlib import Path
import hashlib,json,zipfile
root=Path(__file__).resolve().parent.parent
out=root/'build/usb'
paths={'EFI/TOOLS/BAYTRAIL-USB-IA32.EFI':root/'build/BAYTRAIL-USB-IA32.EFI',
       'EFI/SAMPLES/USBTEST.EFI':out/'BOOTX64.EFI'}
readme=r'''EDK2 X64 Shell z natywnym USB — 2026-10-08

Uruchom EFI/TOOLS/BAYTRAIL-USB-IA32.EFI przez Boot From File lub powłokę IA32.
Podłącz pendrive FAT i klawiaturę USB przed startem.
Loader przechodzi do Shell po wykryciu woluminu USB i klawiatury.
Nie uruchamia automatycznie BOOTX64.EFI i nie wymaga dołączonego testowego pliku.

W Shell wpisz:
  map -r
Znajdź FS z USB(...) w ścieżce. FS0 zwykle jest powłoką z RAM;
numery pozostałych FS zależą od urządzeń. Dla wybranego FS, przykładowo FS1:
  fs1:
  ls
  cd EFI\BOOT
  BOOTX64.EFI
Możesz też wpisać pełną ścieżkę, np. fs1:\EFI\BOOT\BOOTX64.EFI.

Oczekiwane markery: USB FILESYSTEM READY, USB SHELL KEYBOARD READY.
Polecenie echo usb_key_ok daje USB KEYBOARD COMMAND OK.
Dołączony EFI/SAMPLES/USBTEST.EFI to opcjonalna aplikacja testowa X64.
Sprawdza przydzielenie i zwolnienie bloku 160 MiB; loader rezerwuje pulę 256 MiB.
Jej ręczne uruchomienie daje USB MANUAL X64 APP OK; możesz zachować swój plik.
Nie nadpisuj istniejącego BOOTX64 ani BOOTIA32 podczas kopiowania loadera.

USB WriteBlocks pozostaje blokowane. To środowisko PoC z pollingiem i IRQ
wyłączonymi, bez pełnego przejęcia platformy i pełnych runtime services.
Uruchomienie dowolnego EFI nie jest gwarancją obsługi systemu operacyjnego.
W razie zatrzymania zrób zdjęcie całego ekranu.
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
