# Weryfikacja 2026-10-07

Build: LLVM/LLD 23.1.2, NASM 3.02. Kompilacja z -Wall/-Wextra/-Werror,
bez CRT, bez nierozwiązanych symboli. Dwa kolejne buildy dały identyczne
SHA256 dla obydwu EFI, transition.bin, payload.bin i payload.elf.

- M1: PE32, machine 014C, EFI application 10, relokacje obecne, brak imports;
  7680 B; SHA256 `10608c6a486a0577b3d540078ed551511de298dc9c74165b056b68a2dde93c2f`.
- M2: PE32, machine 014C, EFI application 10, relokacje obecne, brak imports;
  10752 B; SHA256 `8a0e7d49a23359e4d6a42a3e5ec4a01d0fac5eebfe632acfe26a012400911e9f`.
- Payload: ELF64 x86-64, entry 0, bez relokacji i named undefined symbols,
  linker assert .bss=0; kod i dane fontu są względne względem położenia obrazu.
- HANDOFF: sizeof=2312, TableRef=16; compile-time assertions obu architektur.
- NASM listing: GDT code32/data/code64, runtime patched GDTR/far target,
  CR0/CR3/CR4/EFER sequence i CALL X64 z RCX oraz aligned stack/shadow space.
- Source locks i commity zainicjalizowanych submodules zgodne; bash syntax
  oraz py_compile skryptów poprawne.
- GOP donor: odrębny PE32+ 8664/subsystem11; metadata/disassembly/VBT report.

Odtworzenie: `./build.sh`; raporty szczegółowe w ignored `build/`.
Sprawdzenie powtarzalności: zachować SHA256SUMS, uruchomić build ponownie,
porównać oba pliki SHA256SUMS bajt po bajcie.

**Ręczny test użytkownika: M1 i M2 potwierdzone zdjęciami przesłanymi
2026-10-07.** Szczegóły i transkrypcja: MANUAL-TEST-2026-10-07.md.
Kompilacja pozostaje kontrolą statyczną; dowodem wykonania jest osobno
zdjęcie `STAGE 2 X64 OK`. Nie zmierzono CR0/CR4/EFER niezależnym debuggerem
ani hashy faktycznie uruchomionych na tablecie plików. Stabilność DMA/AP/SMM
pozostaje niezweryfikowana. 2026-10-08 agent uruchomił M2 i M3 w QEMU
z IA32 OVMF; szczegóły poniżej. Nie wykonano prób Windows.

## Aktualizacja M3 — 2026-10-08

Minimalny EDK2 X64 jest zbudowany i sprawdzony w QEMU IA32 OVMF.
Opis implementacji, zakres testów i instrukcja testu tabletu:
[M3.md](M3.md).

Użytkownik potwierdził M3 na tablecie zdjęciem IMG_0428.
[Zapis testu i jego ograniczenia](MANUAL-TEST-M3-2026-10-08.md).
Bramka do M4 jest spełniona.

## M4 — 2026-10-08

GOP X64 zbudowano i sprawdzono w QEMU IA32 OVMF.
Build, testy, dokładny plik EFI i procedura testu tabletu: [M4.md](M4.md).
M4 potwierdzono IMG_0429; [wynik](MANUAL-TEST-M4-2026-10-08.md).

## Powłoka X64 — 2026-10-08

Oficjalny ShellPkg X64 startuje przez EDK2 z RAM FAT i wykonuje
diagnostykę w QEMU. [Build, zakres i test tabletu](SHELL.md).
Natywne USB/klawiatura pozostają do przygotowania w M5.

## Kandydat M5 USB — 2026-10-08

Natywne xHCI/storage/klawiatura X64: [M5-USB.md](M5-USB.md).
Osobny build-usb.sh i pakiet testowy zawierający dwa pliki EFI; pozytywne testy QEMU
nie potwierdzają jeszcze fizycznego kontrolera tabletu.
