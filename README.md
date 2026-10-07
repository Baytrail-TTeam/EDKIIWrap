# Bay Trail: stock IA32 UEFI → kod X64

Niezależne repozytorium w `/Users/kacper/code/BaytrailProject/baytrail-x64-uefi`.
Dwa sąsiednie repozytoria były użyte wyłącznie jako źródła wiedzy.
CSMWrap/SeaBIOS nie są częścią tego projektu.

Przygotowano dwa ręczne testy:

- `build/BAYTRAIL-M1-IA32.EFI`: zbiera dane, pokazuje `M1 COLLECT OK`, wraca po 30 sekundach.
- `build/BAYTRAIL-M2-IA32.EFI`: ten sam collector, ExitBootServices, własne tablice stron,
  long mode, osadzony kod X64 rysujący `STAGE 2 X64 OK` i zatrzymujący procesor.

**Użytkownik potwierdził M1 i M2 zdjęciami przesłanymi 2026-10-07.**
M1 pokazuje `M1 COLLECT OK`, M2 pokazuje `STAGE 2 X64 OK` po transition.
Opis i granice potwierdzenia: `docs/MANUAL-TEST-2026-10-07.md`.
Agent nie uruchamiał EFI ani nie wykonywał testu fizycznego.
Nie budowano pełnego EDK2 DXE ani nie próbowano Windows.

```sh
./build.sh
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
