# Status — 2026-10-08

| Etap | Implementacja | Weryfikacja wykonania |
|---|---|---|
| M1 — collector IA32 | zbudowany, PE32 IA32, ABI EDK2 | potwierdzony zdjęciem użytkownika IMG_0424 |
| M2 — long mode / wzór X64 | zbudowany, ELF64 i surowy payload osadzony w EFI IA32 | potwierdzony zdjęciem użytkownika IMG_0425 |
| M3 — EDK2 X64 payload | zbudowany: SEC adapter + oficjalny DXE Core/RuntimeDxe + probe | QEMU IA32 OVMF: sukces; tablet: potwierdzony zdjęciem IMG_0428 |
| M4 — GOP w EDK2 | zbudowany fixed-mode GOP X64, FrameBufferBltLib i probe | QEMU IA32 OVMF: sukces; tablet: potwierdzony zdjęciem IMG_0429 |
| Shell X64 — etap dodatkowy | oficjalny ShellPkg, konsola, read-only RAM FAT, LoadImage/StartImage | QEMU IA32 OVMF: sukces; test tabletu oczekuje |
| M5 — USB BOOTX64.EFI | natywne PCI/USB/klawiatura jeszcze niezaimplementowane | bramka M4 spełniona |
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
Shell: docs/SHELL.md; RAM FAT nie potwierdza natywnego USB M5.

Źródła urządzenia: legacy S165/BYT-PF02/Z3736F; etykieta folderu Chuwi Vi10
nie potwierdza modelu capture. Aktywne wyjście w raporcie S165 to eDP; MIPI
jest obecne w VBT, lecz nie potwierdzono aktywnego MIPI. Nie ma wbudowanych
adresów ACPI, framebufferu ani parametrów panelu z tych raportów.

Pozostałe ograniczenia PoC: mapowanie tylko 0–4 GiB, fixed mode framebuffer
32 bpp, maksymalnie 128 ACPI SDT/FACS i bufor mapy 64 KiB. Brak trwałego logu
na pendrive; należy fotografować ekran. Mapę i HANDOFF zachowuje się w RAM.
Brak przejęcia AP, kontrolerów DMA, runtime services, SMM ani reinitializacji
urządzeń; te tematy pozostają do rozwiązania przed bootem OS.
