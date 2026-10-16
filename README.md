# EDKIIWrap

Projekt stworzony dla urządzeń z procesorami Intel Bay Trail-T, który ma umożliwić
przejście z fabrycznego UEFI IA32 do środowiska UEFI X64 opartego na EDK II,
aby uruchamiać 64-bitowe systemy operacyjne (X64).

Projekt jest eksperymentalny; zgodność zależy od urządzenia i uruchamianego systemu.
Według testów użytkownika Linux Mint uruchamia się na tablecie, natomiast problemy
z rozruchem Windows nadal pozostają nierozwiązane.

Dwa sąsiednie repozytoria były użyte wyłącznie jako źródła wiedzy.
CSMWrap/SeaBIOS nie są częścią tego projektu.

Przygotowano pięć ręcznych testów:

- `build/BAYTRAIL-M1-IA32.EFI`: zbiera dane, pokazuje `M1 COLLECT OK`, wraca po 30 sekundach.
- `build/BAYTRAIL-M2-IA32.EFI`: ten sam collector, ExitBootServices, własne tablice stron,
  long mode, osadzony kod X64 rysujący `STAGE 2 X64 OK` i zatrzymujący procesor.
- `build/BAYTRAIL-M3-IA32.EFI`: minimalny EDK2 X64 DXE, testy Boot Services
  i publikacja ACPI/SMBIOS; szczegóły w [docs/M3.md](docs/M3.md).
- `build/BAYTRAIL-M4-IA32.EFI`: GOP X64, testy QueryMode/SetMode/Blt,
  odczyt pikseli i paski RGB; szczegóły w [docs/M4.md](docs/M4.md).
- `build/BAYTRAIL-SHELL-IA32.EFI`: rzeczywista powłoka EDK2 X64 z konsolą
  i read-only FAT w RAM; bez fizycznego USB/klawiatury. [docs/SHELL.md](docs/SHELL.md).

- `build/BAYTRAIL-USB-IA32.EFI` i `build/usb/BOOTX64.EFI`: kandydat M5
  z natywnym USB/storage/klawiaturą X64. [docs/M5-USB.md](docs/M5-USB.md).
  Pakiet testowy: `build/usb/BAYTRAIL-USB-TEST.zip`.

**Użytkownik potwierdził M1 i M2 zdjęciami przesłanymi 2026-10-07.**
M1 pokazuje `M1 COLLECT OK`, M2 pokazuje `STAGE 2 X64 OK` po transition.
Opis i granice potwierdzenia: `docs/MANUAL-TEST-2026-10-07.md`.
M3 uruchomiono poprawnie w QEMU z IA32 OVMF (2026-10-08).
M3 potwierdzono również zdjęciem użytkownika IMG_0428 (2026-10-08):
[wynik testu](docs/MANUAL-TEST-M3-2026-10-08.md). Bramka do M4 spełniona.
M4 potwierdzono również na tablecie zdjęciem IMG_0429 (2026-10-08):
[wynik testu](docs/MANUAL-TEST-M4-2026-10-08.md). Dalszy postęp M5 opisuje `STATUS.md`.
Agent nie wykonywał testów fizycznych; wyniki z tabletu pochodzą od użytkownika.

```sh
./build.sh      # M1/M2
./build-m3.sh   # M3
./build-m4.sh   # M4
./build-shell.sh # Shell X64 z RAM; dodatkowy etap przed natywnym USB M5
./build-usb.sh  # kandydat M5, native USB X64 i testowy BOOTX64.EFI
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
