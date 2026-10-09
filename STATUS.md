# Status — 2026-10-08

| Etap | Implementacja | Weryfikacja wykonania |
|---|---|---|
| M1 — collector IA32 | zbudowany, PE32 IA32, ABI EDK2 | potwierdzony zdjęciem użytkownika IMG_0424 |
| M2 — long mode / wzór X64 | zbudowany, ELF64 i surowy payload osadzony w EFI IA32 | potwierdzony zdjęciem użytkownika IMG_0425 |
| M3 — EDK2 X64 payload | zbudowany: SEC adapter + oficjalny DXE Core/RuntimeDxe + probe | QEMU IA32 OVMF: sukces; tablet: potwierdzony zdjęciem IMG_0428 |
| M4 — GOP w EDK2 | zbudowany fixed-mode GOP X64, FrameBufferBltLib i probe | QEMU IA32 OVMF: sukces; tablet: potwierdzony zdjęciem IMG_0429 |
| Shell X64 — etap dodatkowy | oficjalny ShellPkg, konsola, read-only RAM FAT, LoadImage/StartImage | QEMU IA32 OVMF: sukces; tablet: potwierdzony zdjęciem IMG_0430 |
| M5 — USB BOOTX64.EFI | ograniczony PCI/DMA, timer kooperacyjny, oficjalny stos xHCI/USB/FAT/klawiatury; obecnie bezpośrednio do Shell, ręczny wybór EFI | QEMU: ręczne wykonanie app USB + klawiatura; tablet: IMG_0441–0445 potwierdzają USB FAT i interaktywny Shell w dwóch runach; Ventoy BOOTX64 kończy out of memory |
| M6 — bootmgfw.efi X64 | bez prób | wstrzymany |

Pakiet przygotowano i sprawdzono statycznie; następnie użytkownik przesłał
zdjęcia sukcesu M1 i M2. Napis X64 potwierdza wykonanie minimalnego payloadu
i zachowanie framebufferu po EBS. Dokładne hashe uruchomionych plików nie są
widoczne na zdjęciach. Szczegóły: docs/MANUAL-TEST-2026-10-07.md.
Agent nie wykonywał fizycznych testów, rebootów, flashowania ani zapisów
na tablecie. M2, M3, M4 i Shell X64 sprawdzono również w QEMU; brak prób Windows.
M3: docs/M3.md, docs/M3-QEMU-2026-10-08.json oraz
docs/MANUAL-TEST-M3-2026-10-08.md (ręczny test użytkownika).
M4: docs/M4.md; M4 potwierdzono zdjęciem IMG_0429: docs/MANUAL-TEST-M4-2026-10-08.md.
Shell: docs/SHELL.md; test tabletu: docs/MANUAL-TEST-SHELL-2026-10-08.md.
RAM FAT nie potwierdza natywnego USB M5. Nowy kandydat: docs/M5-USB.md.

Źródła urządzenia: legacy S165/BYT-PF02/Z3736F; etykieta folderu Chuwi Vi10
nie potwierdza modelu capture. Aktywne wyjście w raporcie S165 to eDP; MIPI
jest obecne w VBT, lecz nie potwierdzono aktywnego MIPI. Nie ma wbudowanych
adresów ACPI, framebufferu ani parametrów panelu z tych raportów.

Pozostałe ograniczenia PoC: mapowanie tylko 0–4 GiB, fixed mode framebuffer
32 bpp, maksymalnie 128 ACPI SDT/FACS i bufor mapy 64 KiB. Brak trwałego logu
na pendrive; należy fotografować ekran. Mapę i HANDOFF zachowuje się w RAM.
M5 inicjalizuje wyłącznie xHCI i DMA jego własnych buforów. Brak przejęcia AP,
pozostałych kontrolerów DMA, pełnych runtime services i SMM platformy;
te tematy pozostają do rozwiązania przed bootem OS.

Ręczny tryb USB Shell na tablecie: docs/MANUAL-TEST-M5-USB-SHELL-2026-10-08.md.
Uruchomienie Ventoy pozostaje nieukończone; USB arena EDK2 ma teraz 256 MiB,
plik użytkownika uruchamia bare GRUB w QEMU również przy 64 MiB;
OOM pełnego Ventoy na tablecie nie odtworzono. 2026-10-09 użytkownik zgłosił
wejście do Ventoy po zmianie na 256 MiB, następnie brak reakcji klawiatury
lub zawieszenie (nierozróżnione). W QEMU odtworzono brak wejścia w GRUB
z jego pliku; pompowanie timerów w ReadKeyStroke/ReadKeyStrokeEx przywraca
wykonanie wpisanej komendy. Nowy loader wymaga sprawdzenia na tablecie.
Dowody: docs/M5-USB-KEYBOARD-2026-10-09.json.

2026-10-09: klawiatura w Ventoy potwierdzona przez użytkownika. Windows 10
x64 kończy się BlInitializeLibrary 0xc0000001, na zdjęciach dodatkowo błąd
Pci.Map w xHCI; po powrocie do menu klawiatura nie działa. Aktualny loader
jest kandydatem diagnostycznym: zatrzymuje pierwszy błąd mapowania, podaje
status/adres/rozmiar/CR3. Nie stanowi naprawy startu Windows. Szczegóły i
zdjęcia: docs/MANUAL-TEST-M5-WINDOWS-2026-10-09.json oraz docs/M5-USB.md.
Zdjęcie diagnostyczne DMA: EFI_UNSUPPORTED dla 1 bajtu BusMasterRead, adres
poniżej 4 GiB, CR3 bez zmiany. Z kodu wynika odrzucenie alokacji bounce poza
areną. Ograniczono AllocateMaxAddress do jej końca; kontrola Owned pozostaje.
Nie potwierdzono jeszcze poprawy na tablecie ani naprawy BlInitializeLibrary.
Kolejna próba DMA-BOUND: zdjęcia pokazują powtarzający się BlInitializeLibrary
0xc0000001 i zwrot EFI_NO_MAPPING z obrazu, bez zatrzymania USB DMA MAP FAILED.
To nie potwierdza startu Windows ani stanu klawiatury po powrocie. Pełny ISO
jest potrzebny do odtworzenia tego etapu. Raport:
docs/MANUAL-TEST-M5-WINDOWS-NO-MAP-2026-10-09.json.
2026-10-09: odczytano oba pendrive'y, właściwy loader potwierdzony hashem;
Win10_22H2_English_x64v1.iso dostępny, bezpośrednio odczytany w QEMU tylko RO.
Nasz Shell widzi pełny ISO i wywołuje Windows EFI. Próba kończy w obsłudze
wyjątku DxeCore/CpuDeadLoop (CR2 ffffffe000), bez odtworzenia dokładnego
BlInitializeLibrary z tabletu. Referencyjny X64 OVMF pokazuje graficzne logo
bootu; nie potwierdzono instalatora/OS. Raport: docs/M5-WINDOWS-ISO-2026-10-09.json.
Ventoy bezpiecznie wysunięty na prośbę użytkownika; plików USB nie zmieniano.
