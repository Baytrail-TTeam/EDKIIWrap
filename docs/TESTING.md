# Pierwszy test ręczny — wykonuje właściciel tabletu

Agent uruchomił M2/M3 w QEMU IA32 OVMF; nie wykonywał testów fizycznych.
Użytkownik przesłał zdjęcia sukcesu M1/M2; zapis wyników: MANUAL-TEST-2026-10-07.md. Nie zmieniaj BootOrder,
SPI ani partycji eMMC. Użyj istniejącej opcji jednorazowego Boot From File
lub istniejącego IA32 EFI Shell. Potrzebny jest firmware dopuszczający
niesygnowany testowy EFI; ten projekt nie zapewnia obejścia Secure Boot.

## Najpierw M1

1. Na komputerze skopiuj `build/BAYTRAIL-M1-IA32.EFI` do głównego katalogu
   istniejącego pendrive FAT32. Nie zmieniaj jego innych plików ani fallback EFI.
2. Samodzielnie wybierz w firmware Boot From File → pendrive →
   `BAYTRAIL-M1-IA32.EFI`. Jeżeli używasz istniejącego IA32 shell, ustal właściwe
   `fsN:` przez `map -r`, następnie uruchom `fsN:\BAYTRAIL-M1-IA32.EFI`.
   Numer fsN nie jest z góry znany. Nie używaj bcfg ani ustawiania BootOrder.
3. Oczekuj nagłówka `M1 COLLECT ONLY`, CPUID, adresu framebufferu,
   width/height/stride/pixel format, adresów ACPI/SMBIOS, liczby tabel i
   `M1 COLLECT OK`. Wartości są zapisane szesnastkowo: 1366=0x556,
   768=0x300, 1376=0x560. Flags 1=ACPI, 2=SMBIOS, 3=oba.
4. Zrób fotografię ekranu w ciągu 30 sekund. Aplikacja wraca do wywołującego;
   dalsza polityka bootowania należy do firmware. M1 nie wywołuje EBS i nie
   przełącza CPU w long mode.
5. Przekaż wynik M1. Przy błędzie lub braku wymaganych danych zatrzymaj
   procedurę; nie uruchamiaj M2. M1 jest pierwszą bramką do potwierdzenia.

## M2 — dopiero po zweryfikowaniu M1

Skopiuj `build/BAYTRAIL-M2-IA32.EFI` i wybierz jednorazowo ten plik.
Aplikacja zawiera transition i payload; nie potrzebuje żadnego .bin na USB.

Oczekiwany przebieg: `M2 TRANSITION` → informacje collector →
`M2: ExitBootServices then long mode` → mały granatowy prostokąt w lewym
górnym rogu, zielona górna linia i biały napis **STAGE 2 X64 OK**.
Pozostała część framebufferu nie jest czyszczona przez payload.
Napis rysuje kod skompilowany dla X64 przez bezpośrednie zapisy pikseli,
bez usług stockowego GOP i bez wywołania funkcji EFI.

Po napisie CPU pozostaje w CLI/HLT. Nie działa shell ani klawiatura testu;
nie ma automatycznego powrotu, rebootu ani shutdown. Właściciel wykonuje
własną normalną procedurę zakończenia testu. Fotografia napisu potwierdza
jedynie wykonanie M2: nie potwierdza działania EDK2/USB/Windows.

## Możliwe awarie

| Objaw | Co ustala / dalszy krok |
|---|---|
| Firmware nie uruchamia pliku | sprawdzić ręczny wybór IA32 i politykę podpisów; PE32 potwierdzono statycznie |
| PRE-EXIT ERROR | błąd EFI przed EBS; zdjęcie dokładnego kodu i ostatniego komunikatu, bez M2 |
| Invalid/unmapped ACPI or SMBIOS | checksum, rozmiar, zakres pamięci, limit 128 tabel lub adres >4 GiB; potrzebna analiza mapy |
| Unsupported CPU/mode/PAT | brak wymaganych cech, EFER.LMA albo niestandardowy PAT[3]; nie zmieniać MSR ręcznie |
| Brak GOP / PixelBltOnly / format 16 bpp / framebuffer >4 GiB | PoC odrzuca tryb; nie reinitializować GPU |
| Ostatnia linia EBS i zatrzymanie | możliwy błąd EBS, wyjątek transition, problem mapowania; sam ekran nie rozróżnia tych przyczyn |
| Reset lub czarny ekran | możliwy triple fault, NMI/SMI, firmware display teardown, DMA/AP lub zła dostępność framebufferu; nie oznacza pewnej awarii GOP |
| Kolory lub napis błędne | pitch/pixel masks/cache/display state; przekazać zdjęcie i wyniki M1 |
| STAGE 2 X64 OK | M2 dotarło do kodu X64; zapisać nazwę i SHA256 pliku oraz zdjęcie |

M3 zawiera diagnostykę ASSERT i został sprawdzony w QEMU z IA32 OVMF. X64 OVMF nie sprawdza wejścia ze stock IA32;
QEMU nie emuluje MIPI/eDP/PMIC tego tabletu. Automatyczny test QEMU jest dostępny w scripts/test-m3-qemu.py.
Testy fizyczne wykonuje użytkownik.

## Aktualizacja M3 — 2026-10-08

Minimalny EDK2 X64 jest zbudowany i sprawdzony w QEMU IA32 OVMF.
Opis implementacji, zakres testów i instrukcja testu tabletu:
[M3.md](M3.md).
