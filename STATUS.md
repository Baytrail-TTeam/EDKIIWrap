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
OOM pełnego Ventoy na tablecie nie odtworzono. Próba 256 MiB oczekuje użytkownika.
