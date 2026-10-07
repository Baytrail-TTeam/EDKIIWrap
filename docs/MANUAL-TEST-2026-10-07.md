# Wynik M1 i M2 — zdjęcia użytkownika

Dowody otrzymano 2026-10-07: `/Users/kacper/Downloads/IMG_0424.HEIC`
i `/Users/kacper/Downloads/IMG_0425.HEIC`. Agent jedynie odczytał zdjęcia;
nie wykonywał testu na tablecie. Daty wykonania zdjęć, wersji płyty oraz
hashy uruchomionych plików nie ustalono. Hashe oryginalnych HEIC i dane
strukturalne są w `MANUAL-TEST-2026-10-07.json`; nie kopiowano zdjęć do Git.

## M1 — potwierdzony collector

IMG_0424 pokazuje nagłówek `M1 COLLECT ONLY` i `M1 COLLECT OK`.

| Pole | Wartość widoczna |
|---|---|
| CPUID | 0x30678 |
| Framebuffer | 0x80000000 |
| Rozdzielczość | 0x556 × 0x300 = 1366 × 768 |
| PixelsPerScanLine | 0x560 = 1376 |
| PixelFormat | 1, PixelBlueGreenRedReserved8BitPerColor |
| ACPI2 RSDP | 0x7CCF6014 |
| ACPI1 RSDP | 0, brak osobnego wpisu |
| Liczba rekordów ACPI | 0x1C = 28 |
| SMBIOS2 entry point | 0x7BD22000 |
| SMBIOS3 | 0, brak wpisu |
| Memory map bytes | 0x810 = 2064 |
| HANDOFF | 0x7B8EB000 |
| Presence flags | 3, ACPI i SMBIOS |

Według bieżącego kodu komunikat sukcesu występuje po walidacji zakresów
i checksums oraz przejściu przez RSDT/XSDT i FADT do DSDT/FACS. Fotografia
nie jest dumpem tabel ani mapy; nie potwierdza ich bajtowej tożsamości
z wcześniejszym capture. Powrotu do firmware po 30 sekundach nie widać.

## M2 — potwierdzony minimalny kod X64

IMG_0425 pokazuje granatowy prostokąt z zieloną górną linią i napisem
`STAGE 2 X64 OK`. Pod nim jest komunikat IA32:
`M2: ExitBootServices then long mode. No EFI calls afterwards.`

W bieżącej implementacji tylko X64 payload rysuje ten wzór. Dochodzi do
niego po udanym ExitBootServices, przygotowaniu paging/long mode i CALL
z RCX wskazującym HANDOFF. Zdjęcie potwierdza dojście do tego kodu oraz
użyteczność odziedziczonego framebufferu dla pokazanych zapisów pikseli.
Nie jest niezależnym odczytem rejestrów ani pełną walidacją platformy.

Widoczne ACPI/SMBIOS, rozdzielczość, pitch i format są takie jak w M1.
Map size wydrukowane przed ostatnim GetMemoryMap to 0x7E0 = 2016 B;
HANDOFF to 0x7B8EB000. Różne rozmiary map między uruchomieniami nie są
same w sobie błędem: układ alokacji i scalanie deskryptorów mogą się różnić.
Ostatecznej mapy użytej do EBS nie pokazuje ta fotografia.

Adresy i geometria są zgodne z historycznymi raportami S165. Fotografie
nie pokazują DMI/modelu ani board revision, więc nie przypisują urządzeniu
nazwy Chuwi Vi10 i nie dowodzą aktywnego MIPI.

## Status bramek

M1 i M2 mają dowód wykonania przez użytkownika. Następny etap to M3:
minimalny EDK2 X64 payload z poprawnymi HOB/FV i rezerwacjami pamięci.
W tej aktualizacji zapisano wyniki; nie budowano ani nie uruchamiano M3.
Nie potwierdzono jeszcze protokołu GOP X64, sterowników USB/eMMC,
ładowania BOOTX64.EFI, Windows ani stabilności AP/DMA/SMM. Powtarzalności
na kilku cold/warm boots i długości pozostawania w HLT nie ustalono.
