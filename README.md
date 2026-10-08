# Bay Trail: stock IA32 UEFI → kod X64

Niezależne repozytorium w `/Users/kacper/code/BaytrailProject/baytrail-x64-uefi`.
Dwa sąsiednie repozytoria były użyte wyłącznie jako źródła wiedzy.
CSMWrap/SeaBIOS nie są częścią tego projektu.

Przygotowano cztery ręczne testy:

- `build/BAYTRAIL-M1-IA32.EFI`: zbiera dane, pokazuje `M1 COLLECT OK`, wraca po 30 sekundach.
- `build/BAYTRAIL-M2-IA32.EFI`: ten sam collector, ExitBootServices, własne tablice stron,
  long mode, osadzony kod X64 rysujący `STAGE 2 X64 OK` i zatrzymujący procesor.
- `build/BAYTRAIL-M3-IA32.EFI`: minimalny EDK2 X64 DXE, testy Boot Services
  i publikacja ACPI/SMBIOS; szczegóły w [docs/M3.md](docs/M3.md).
- `build/BAYTRAIL-M4-IA32.EFI`: GOP X64, testy QueryMode/SetMode/Blt,
  odczyt pikseli i paski RGB; szczegóły w [docs/M4.md](docs/M4.md).

**Użytkownik potwierdził M1 i M2 zdjęciami przesłanymi 2026-10-07.**
M1 pokazuje `M1 COLLECT OK`, M2 pokazuje `STAGE 2 X64 OK` po transition.
Opis i granice potwierdzenia: `docs/MANUAL-TEST-2026-10-07.md`.
M3 uruchomiono poprawnie w QEMU z IA32 OVMF (2026-10-08).
M3 potwierdzono również zdjęciem użytkownika IMG_0428 (2026-10-08):
[wynik testu](docs/MANUAL-TEST-M3-2026-10-08.md). Bramka do M4 spełniona.
M4 zbudowano i sprawdzono w QEMU; test na tablecie pozostaje do wykonania.
Agent nie wykonywał testów fizycznych. M5 i próby Windows są wstrzymane.

```sh
./build.sh      # M1/M2
./build-m3.sh   # M3
./build-m4.sh   # M4
```

Toolchain: LLVM/LLD 23.1.2, NASM 3.02, Python 3. Źródła EDK2 `edk2-stable202608`
przypięte do commitów w `third_party/sources.lock.json`. Patrz `docs/BUILD.md`.

- `docs/TESTING.md`: dokładna procedura i awarie.
- `docs/ARCHITECTURE.md`: kontrakt HANDOFF, transition i plan dalszej integracji.
- `docs/RESEARCH.md`: źródła urządzenia, EDK2/coreboot/MrChromebox, ograniczenia.
- `docs/GOP-DONOR.json`: znaleziony GOP X64, hashe i companion VBT.
- `STATUS.md`: status etapów i bramki testowe.

Nie kopiowano stockowych dumpów, prywatnych ACPI ani identyfikatorów do nowego repo.
Pobrane third_party i build są ignorowane przez Git. Prawa do dalszej dystrybucji
firmware donorowego nie zostały ustalone; nie publikować blobów automatycznie.
