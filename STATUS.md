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

2026-10-09, kandydat MEM-RTC: znaleziono błąd HOB rezerwacji. PRESENT oraz
INITIALIZED bez TESTED pozwalały DXE PromoteMemoryResource zmieniać stock
reserved RAM w wolną pamięć po nieudanej alokacji. Kontrolna próba alokacji
0x102000/16 stron na poprzedniej wersji kończy się page fault; po dodaniu
TESTED rezerwacje pozostają zamknięte, test oraz alokacja 160 MiB przechodzą.
Wyjątki CPU są teraz wyświetlane bezpośrednio na framebufferze, z CR2/CR3.

Drugi wykryty brak: GetTime zwracał EFI_UNSUPPORTED. ReadOnlyRtc odczytuje
istniejący zegar CMOS bez ustawiania daty, trybu lub alarmów. W kontrolnym
teście samego Windows BOOTX64.EFI komunikat BlInitializeLibrary 0xc0000001
znika po tej zmianie. Obraz nadal wraca z EFI_NO_MAPPING; brak pełnego ISO
po wysunięciu Ventoy uniemożliwia ocenę dalszego startu. Nie potwierdzono
Windows/WinPE ani poprawki na tablecie. Siedem testów QEMU z asercjami
przechodzi; osobno sprawdzono klawiaturę w izolowanym GRUB użytkownika.
Loader: build/BAYTRAIL-USB-IA32-MEM-RTC.EFI. Dowody:
docs/M5-MEM-RTC-2026-10-09.json. Nie zmieniano fizycznych pendrive'ów.

2026-10-09: nowe zdjęcie użytkownika pokazuje Windows Boot Manager i
0xc0000017: za mało pamięci na ramdisk. Jest to kolejny etap po poprzednim
BlInitializeLibrary; uruchomienia WinPE nadal nie potwierdzono. Poprzednia
pula własna wynosiła tylko 256 MiB. Kandydat 1G zwiększa ją do 1024 MiB
przy zachowaniu stock rezerwacji i przydziale całości przez IA32 AllocatePages.
Test dużego ciągłego bloku 640 MiB, Shell/USB/klawiatura, hub-MBR, removable
oraz odrzucenie błędnego DMA przechodzą w QEMU z 1536 MiB fizycznego RAM.
Izolowany GRUB użytkownika przyjmuje komendę. Nowy plik:
build/BAYTRAIL-USB-IA32-1G.EFI. Nie testowano go jeszcze na tablecie ani
z pełnym ISO Windowsa; szczegóły: docs/M5-WINDOWS-RAMDISK-2026-10-09.json.

2026-10-09: po kandydacie 1G użytkownik zgłasza Windows 7 page fault
(IMG_0466: RIP 41cedaf2, CR2 fffff80042200028, CR3 7b8b5000) oraz Windows 10
stojący >2 min na pustym niebieskim tle (IMG_0467). Tryb Ventoy Normal.
Nie zidentyfikowano modułu win7 ani fazy zatrzymania win10; brak kodu BSOD.
Osobno znaleziono brak CPU Arch wymaganej przez CoreExitBootServices.
Próba kontrolna starego loadera kończy się invalid opcode przy RCX=0;
kandydat CPU-EXIT przechodzi ExitBootServices, zeruje BootServices w ST i
pozostawia IRQ wyłączone. CPU Arch ograniczona do rzeczywistych metod
DisableInterrupt, GetInterruptState i WriteBackInvalidate; pozostałe
operacje odrzucane. To nie jest pełny CpuDxe ani dowód naprawy Windowsa.
Nowy marker pokazuje status zwrotu ExitBootServices. Direct (640 MiB), hub-MBR,
DMA rejection i klawiatura w izolowanym GRUB sprawdzone w QEMU.
Plik: build/BAYTRAIL-USB-IA32-CPU-EXIT.EFI. Raport: docs/M5-CPU-EXIT-2026-10-09.json.
Kolejna próba fizyczna: użytkownik potwierdza Windows 7 pokazujący EFI EXIT
BOOT SERVICES STATUS 0, bez zmiany ponad dwie minuty. Zwrot EBS potwierdzony
na tablecie, ale wejście do kernela, SetVirtualAddressMap i dalszy handoff
nie zostały potwierdzone. Nie otrzymano świeżego zdjęcia ani nowego faultu.
Następny kontrolowany test: ten sam CPU-EXIT i Windows 10 w Ventoy WIMBOOT,
zmieniając tylko tryb startu wobec wcześniejszego Normal. Pełne źródłowe ISO
oraz pozostałe pliki Ventoy nadal nie są dostępne lokalnie do odtworzenia.
IMG_0471 po prośbie o test WIMBOOT ponownie pokazuje Boot Manager
0xc0000017 i brak pamięci na ramdisk. Tryb i hash loadera nie są widoczne
na zdjęciu, przyjęto kontekst rozmowy. Nie zwiększono dalej areny bez
pomiaru żądanej/dostępnej ciągłej pamięci i rozmiaru oryginalnego boot.wim.
Poproszono o ponowne podłączenie Ventoy do Maca; odrębny polski obraz
zamontowany na Macu nie jest oryginalnym angielskim ISO użytkownika.
Raport: docs/M5-WIMBOOT-RAMDISK-2026-10-09.json.

2026-10-12: oryginalny Win10 ISO ponownie dostępny; kopia lokalna ma znany
hash a6f470ca..., boot.wim 677539713 bajtów (646.15 MiB). Użytkownik wskazał
Win7 Patched ACPI and Updates; także skopiowany tylko odczytem.
Pełny Win10 na CPU-EXIT odtwarza bugcheck 5C / 8000 / B / 0 / 0.
Exact ntoskrnl + symbole Microsoft + QEMU trace: HalpAcpiFindRsdp znajduje
ACPI, lecz HalpAcpiAllocateMemory nie ma stron poniżej 16 MiB i zwraca
C000009A; później zawodzi inicjalizacja przerwań. Standardowe X64 OVMF
pokazuje Setup. Samo poprawienie stock ACPI/NVS/runtime typów mapy nie usuwa
5C; dopiero osobna pula 4 MiB poniżej 16 MiB pozwala dojść do Setup.

LOWMEM: dodatkowa pula jawnie przydzielona przez stock IA32 AllocateMaxAddress;
main 1024 MiB pozostaje. M3 v2, 2400 bajtów; zakresy free RAM/DMA ograniczone
do obu przydzielonych pul. Stock ACPI/NVS/runtime zajęte typed HOB-ami na
SystemMemory (GCD Reserved ignorował ich typy), z kontrolą braku wolnej RAM
poza pulami. Osiem testów z asercjami przechodzi; GRUB sprawdzony osobno.
Win10 Setup w QEMU po 120 s reaguje na Tab, bugcheck data zerowe.
Kandydat: build/BAYTRAIL-USB-IA32-LOWMEM.EFI. Nie instalowano OS; test na
tablecie, pełnym Ventoy i Win7 nadal wymagany. Fizycznych USB nie zmieniano.
Raport: docs/M5-WINDOWS-LOWMEM-2026-10-12.json oraz PNG BEFORE/AFTER/REFERENCE/USB.
