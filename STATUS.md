# Status — 2026-10-07

| Etap | Implementacja | Weryfikacja wykonania |
|---|---|---|
| M1 — collector IA32 | zbudowany, PE32 IA32, ABI EDK2 | oczekuje ręcznego testu |
| M2 — long mode / wzór X64 | zbudowany, ELF64 i surowy payload osadzony w EFI IA32 | oczekuje M1 i ręcznego testu M2 |
| M3 — EDK2 X64 payload | analiza wejścia/HOB, bez implementacji | wstrzymany do M2 |
| M4 — GOP w EDK2 | znaleziony gotowy GraphicsOutputDxe do adaptacji | wstrzymany |
| M5 — USB BOOTX64.EFI | plan sterowników | wstrzymany |
| M6 — bootmgfw.efi X64 | bez prób | wstrzymany |

Granica tej sesji: przygotowanie pierwszego pakietu testowego i analiza statyczna.
Brak testu fizycznego i QEMU. Kompilacja nie dowodzi wejścia w long mode.
Brak flashowania, rebootu, zmiany BootOrder, zapisów eMMC czy operacji na tablecie.
Kod transition zawiera wymagany zapis EFER.LME, lecz nie został wykonany.

Źródła urządzenia: legacy S165/BYT-PF02/Z3736F; etykieta folderu Chuwi Vi10
nie potwierdza modelu capture. Aktywne wyjście w raporcie S165 to eDP; MIPI
jest obecne w VBT, lecz nie potwierdzono aktywnego MIPI. Nie ma wbudowanych
adresów ACPI, framebufferu ani parametrów panelu z tych raportów.

Pozostałe ograniczenia PoC: mapowanie tylko 0–4 GiB, fixed mode framebuffer
32 bpp, maksymalnie 128 ACPI SDT/FACS i bufor mapy 64 KiB. Brak trwałego logu
na pendrive; należy fotografować ekran. Mapę i HANDOFF zachowuje się w RAM.
Brak przejęcia AP, kontrolerów DMA, runtime services, SMM ani reinitializacji
urządzeń; te tematy muszą zostać rozwiązane przed M3/Windows.
