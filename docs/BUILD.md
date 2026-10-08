# Powtarzalny build

W tej sesji użyto macOS Apple Silicon; cross-compile nie wymaga tabletu.
Zainstalowano Homebrew LLVM 23.1.2 i LLD 23.1.2. Homebrew przy instalacji
LLVM zaktualizował zależności xz i z3. Istniejący NASM: 3.02.
Nie zmieniano konfiguracji shell ani istniejących repozytoriów.

```sh
brew install llvm lld nasm python
cd /Users/kacper/code/BaytrailProject/baytrail-x64-uefi
python3 scripts/fetch-sources.py
./build.sh
```

Pierwsza komenda instaluje bieżące wersje: dla identycznych bajtów należy
użyć wersji z `build/manifest.json`, nie dowolnych przyszłych wersji.
Plik sources.lock przypina EDK2 i research trees do commitów oraz cztery
zainicjalizowane submodules (Brotli dla BaseTools i decompressor, libfdt,
MipiSysT). Pozostałe zależności pełnego firmware nie są wymagane przez M1/M2.
`build.sh` używa nagłówków EDK2; odrębny `build-m3.sh` kompiluje BaseTools
i minimalną platformę DXE opisaną w M3.md.

Dla innej instalacji ustaw LLVM_BIN/LLD_BIN, opcjonalnie CC/OBJCOPY:

```sh
LLVM_BIN=/path/to/llvm/bin LLD_BIN=/path/to/lld/bin ./build.sh
```

Build sprawdza lock, NASM emituje transition + listing. Clang kompiluje
payload `x86_64-none-elf` freestanding PIE bez SSE, red zone i bibliotek.
ld.lld wiąże ELF64 z entry 0; llvm-objcopy emituje raw image, a embed.py
osadza obydwa bloby. IA32 aplikacje są COFF `i686-pc-windows-msvc` z ABI
EFI i CHAR16 przez -fshort-wchar. lld-link ustawia EFI application, relokacje
PE, timestamp 0, brak CRT/importów. Sam payload nie jest osobną aplikacją EFI.

Artefakty: `build/BAYTRAIL-M1-IA32.EFI`, `build/BAYTRAIL-M2-IA32.EFI`,
`payload.elf`, `payload.bin`, `transition.bin`, `transition.lst`.
Raporty: `verification.txt`, `pe-report.txt`, `payload-disassembly.txt`,
`payload-report.txt`, `manifest.json`, `SHA256SUMS`.

`verify.py` sprawdza PE machine 014C, PE32, subsystem 10, entry, timestamp,
relokacje oraz ELF class/machine/entry i brak named unresolved symbols oraz
relokacji payloadu. Powtórzenie buildu z porównaniem pięciu hashy potwierdza
powtarzalność w użytym środowisku. Nie potwierdza poprawności runtime ani
zgodności firmware. Kompilacja na Linuksie nie była testowana.

GOP donor jest analizowany osobno: `python3 scripts/analyze-gop.py`.
Nie jest osadzany w testowym EFI. Duże dumpy są wyłącznie w starych repo.

## Aktualizacja M3 — 2026-10-08

Minimalny EDK2 X64 jest zbudowany i sprawdzony w QEMU IA32 OVMF.
Opis implementacji, zakres testów i instrukcja testu tabletu:
[M3.md](M3.md).

## M4 — 2026-10-08

GOP X64 zbudowano i sprawdzono w QEMU IA32 OVMF.
Build, testy, dokładny plik EFI i procedura testu tabletu: [M4.md](M4.md).
M5 oczekuje na potwierdzenie M4 przez użytkownika.
