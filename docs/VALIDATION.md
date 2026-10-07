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

**Brak wykonania EFI, QEMU, testu tabletu i Windows.** Nie jest to dowód
poprawności transition, dostępności framebufferu po EBS, bezpieczeństwa
DMA/AP/SMM ani działania sterowników. Te wyniki pozostają do ręcznego
potwierdzenia zgodnie z TESTING.md. M3 nie został rozpoczęty.
