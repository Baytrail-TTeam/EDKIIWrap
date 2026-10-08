# Uruchomienie prawdziwej powłoki EDK2 X64

2026-10-08: przygotowano samodzielny `BAYTRAIL-SHELL-IA32.EFI`.
QEMU z IA32 OVMF uruchamia oficjalny ShellPkg X64 przez nowe Boot Services
EDK2: LoadImage/StartImage. Widać banner `UEFI Interactive Shell v2.2`,
wynik `ver`, mapowanie `FS0:` i prompt `Shell>`.
Test na tablecie pozostaje do wykonania przez użytkownika.

To dodatkowy etap uruchomienia aplikacji/konsoli po potwierdzonym M4.
**Nie oznacza ukończenia M5: fizyczne USB, klawiatura i BOOTX64.EFI z
pendrive'a obsługiwanego przez X64 wymagają jeszcze natywnego stosu.**
Powłoka w tym pakiecie uruchamia się z osadzonego, read-only FAT w RAM.
Po przejściu nie wywołujemy stockowych IA32 protokołów.

## Co jest uruchamiane

Zachowany collector/transition/HANDOFF/SEC wchodzi do oficjalnego DXE Core.
FV zawiera RuntimeDxe, emulator zmiennych VariableRuntimeDxe,
HiiDatabase, GraphicsConsoleDxe, EnglishDxe, DiskIoDxe, FatDxe, nasz GOP
z M4 oraz mały dispatcher ShellBoot. Oficjalny ShellPkg ma podstawowe
biblioteki poleceń Level1/2/3; zbudowany jako PE32+ X64 application.

Dispatcher ponawia testy M3, podłącza GraphicsConsole do GOP, ustawia
ConOut/StdErr w nowej SystemTable oraz jej CRC. FirmwareVendor wskazuje
własny string w RuntimeServicesData. Ustawia też wirtualne SimpleTextIn,
które zwraca EFI_NOT_READY: w tym teście nie ma fizycznej klawiatury.
Shell może informować o braku SimpleTextInputEx — to oczekiwane ograniczenie.

Deterministyczny obraz FAT16 ma 4 MiB; do FV osadzony jest jego niezerowy
początkowy fragment, a read-only BlockIo zeruje pozostałą część przy odczycie.
Dispatcher sprawdza odczyt sektora, MediaId, rozmiar/granice i odmowę zapisu.
Ten BlockIo instaluje się na osobnym vendor DevicePath. Oficjalne DiskIo/FAT
publikują SimpleFileSystem. Jest to cały wirtualny wolumin, bez partycji hosta.

Następnie EDK2 wykonuje LoadImage z ścieżki
`\EFI\BOOT\BOOTX64.EFI` tego FAT i StartImage. To rzeczywisty ShellPkg,
a nie imitacja tekstu powłoki. `startup.nsh` wykonuje `ver`, `map -r` i echo
`SHELL_X64_RAM_OK`. Po zarejestrowaniu tego tekstu na ConOut i pierwszym
idle wait dispatcher rysuje zielone `SHELL X64 SCRIPT OK` i kończy w CLI/HLT.
Nie podajemy sztucznych naciśnięć klawiszy. Prompt jest widoczny, ale nie
można w nim jeszcze wpisać polecenia. Nie ma automatycznego rebootu.

Zmienne, w tym emulowane NV, są przechowywane wyłącznie w RAM:
PcdEmuVariableNvModeEnable=TRUE. Stockowe NVRAM/BootOrder, SPI i eMMC
nie są używane ani wystawiane. BlockIo.WriteBlocks zwraca WRITE_PROTECTED.
Bez inicjalizacji PCI/USB/DMA, bez programowania kontrolerów i IRQ.
Nie ma kompletnego zestawu architectural protocols, sprzętowego timera,
zegara runtime, wejścia USB ani polityki memory map dla OS. Nie używać
tego jako gotowej platformy dla Windows lub innego OS.

## Build i dowód

```sh
./build-shell.sh
python3 scripts/test-shell-qemu.py
```

Narzędzia, przypięte źródła i reguły reproducibility są takie jak dla M3/M4.
Osobny workspace: `build/m5/workspace`; nazwa katalogu nie oznacza
ukończenia M5. Skrypt najpierw kompiluje Shell.dsc, tworzy FAT, buduje
ShellPlatform.dsc/FDF i osadza FV w aplikacji IA32 z ShellLoader.dsc.
SOURCE_DATE_EPOCH jest stały, PE timestamp=0. Ścieżka checkoutu i wersje
narzędzi wpływają na CodeView/PDB; identyczność deklarujemy dla tej samej
lokalizacji i użytego toolchainu.

Raport statyczny i hashe: `build/m5/manifest.json`. Sprawdza PE maszynę,
subsystem, entry, importy/relokacje, FV checksum/FFS/PE sekcje i osadzenie
właściwego FV/SEC/FAT/Shell/transition. FAT metadata/hash:
`build/m5/shell-disk.json`. Logi: `build/m5/*.log`.

QEMU używa prawdziwego IA32 OVMF, własnego ESP i kopii variables. Automat
sprawdza zielony marker powstały po obserwacji outputu skryptu i stan
CS64/HLT=1. Zrzut ekranu sprawdzono dodatkowo wzrokowo: banner powłoki,
`ver`, FS0, token sukcesu i prompt. Screenshot/rejestry/wynik:
`build/m5/qemu-shell/`. Archiwalny raport: `docs/SHELL-QEMU-2026-10-08.json`.
Czysty rebuild porównuje hashe wszystkich EFI oraz FV i obrazów FAT.

## Ręczny test na tablecie

1. Skopiuj **tylko** `build/BAYTRAIL-SHELL-IA32.EFI` na pendrive FAT32,
   np. do `\EFI\TOOLS\BAYTRAIL-SHELL-IA32.EFI`. Obraz FAT i powłoka są
   już osadzone w pliku. Nie zastępuj bootloadera systemu ani starszych testów.
2. Uruchom ręcznie przez tę samą metodę Boot From File / IA32 EFI Shell
   jak M4. Po ustaleniu fsN w Shell:
   `fsN:\EFI\TOOLS\BAYTRAIL-SHELL-IA32.EFI`.
3. Po etapach M3 pojawi się konsola powłoki. Oczekuj
   `UEFI Interactive Shell v2.2`, nazwy `Baytrail EDK2 X64`, mapowania
   `FS0:`, `SHELL_X64_RAM_OK`, promptu `Shell>` oraz zielonego paska
   `SHELL X64 SCRIPT OK` w dolnej części ekranu.
4. Brak reakcji klawiatury i nieruchomy ekran są w tym pakiecie zamierzone.
   Zrób zdjęcie całego ekranu i zanotuj SHA256 kopiowanego pliku.

FS0 jest woluminem RAM; nie jest pendrive'em ani eMMC. Rozdzielczość i
tryb panelu pozostają z M4; tekst może być mniejszy i wyśrodkowany przez
oficjalną GraphicsConsole. Komunikat o braku SimpleTextInputEx jest
oczekiwany. Czerwony `SHELL ... FAILED`, `SHELL IDLE BEFORE SCRIPT`, ASSERT,
czarny ekran bez banneru lub brak końcowego markera wymagają zdjęcia.
Nie przystępujemy jeszcze do Windows. Następna praca M5 to natywne
PCI/USB/klawiatura i odczyt BOOTX64.EFI z fizycznego pendrive'a.
