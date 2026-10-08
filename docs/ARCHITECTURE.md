# Architektura

## M1 / M2

Stock IA32 UEFI inicjalizuje DRAM i wyświetlacz. Nasza aplikacja używa
oficjalnych nagłówków EDK2, zbiera dane i nie ładuje stockowych protokołów
EFI do kodu X64. Nie zmienia trybu GOP.

`include/Handoff.h` definiuje little-endian packed ABI v1, magic
`0x31464f484c545942`, rozmiar 2312 B. Wszystkie adresy mają `uint64_t`;
wskaźniki natywne występują tylko lokalnie w collectorze. ABI jest sprawdzane
przez `_Static_assert` podczas kompilacji obu architektur.

HANDOFF zawiera mapę pamięci z rzeczywistym DescriptorSize/Version,
framebuffer i maski kolorów, oddzielne ACPI 1/2 RSDP i SMBIOS 2/3 entry points,
adresy/rozmiary obszarów SMBIOS, inventory SDT/FACS, CPUID oraz odczytane
CR0/CR4/EFER/PAT, przydziały obrazu/transition/payload/stack/page tables.
Nie przekazuje IA32 SystemTable ani BootServices/RuntimeServices.

Collector sprawdza checksums RSDP/SDT/SMBIOS, rozmiary i zakresy z mapy EFI.
Przechodzi przez obie root tables i FADT do DSDT/FACS, zachowuje wszystkie
SSDT, MADT, MCFG, HPET oraz pozostałe SDT. Nie interpretuje AML, nie ładuje
dynamicznych SSDT i nie patchuje tabel. FACS nie ma checksum SDT.
Adresy >4 GiB i reserved/MMIO backing są odrzucane przed dereferencją;
może to odrzucić prawidłowy, ale nietypowy firmware i wymaga analizy wyniku M1.
Brak ACPI/SMBIOS jest sygnalizowany flags i zerowymi adresami; nie blokuje
minimalnego framebufferowego M2, ale blokowałby przyszły kompletny payload.

## Transition

Przed ExitBootServices przydzielamy poniżej 4 GiB: HANDOFF 1 stronę,
mapę 16 stron, wykonywalny blob transition, wykonywalny payload,
stos 64 KiB oraz 6 stron tablic. Payload ma entry offset 0 i wyłącznie
względne odwołania do kodu/danych. Rozmiary .bss muszą być zerowe.

PML4[0] wskazuje PDPT. Cztery wpisy PDPT wskazują cztery PD; 2048 PDE
z 2 MiB huge pages pokrywa identity 0–4 GiB. Obszary framebufferu mają
PWT/PCD ustawione na PAT index 3; collector wymaga, aby PAT[3] było UC.
MTRR/PAT nie są zmieniane. Nie mapujemy jeszcze indywidualnych ochron
DXE ani przestrzeni powyżej 4 GiB; pozostałe regiony używają PAT index 0.

Ostatni komunikat jest drukowany przed ostatecznym GetMemoryMap i
ExitBootServices. Po nieudanym EBS retry wykonuje wyłącznie GetMemoryMap/EBS;
nie alokuje, nie drukuje i nie wraca do shell. Po maksymalnie trzech
nieudanych próbach zatrzymuje BSP. Po udanym EBS nie ma wywołań stockowego IA32 EFI.
M3 wywołuje później własne Boot Services X64 EDK2.

`transition/long-mode.asm`:

1. CLI, CLD; wyczyszczenie TF/NT/AC w EFLAGS.
2. Wyliczenie własnej bazy przez call/pop, załadowanie własnego GDT;
   retf do flat code32, płaskie DS/ES/SS/FS/GS.
3. Przygotowanie fault-only IDT32 i IDT64 (wektory 0–31).
4. CR0.PG=0; CR4.PAE=1; CR3=PML4; EFER.LME=1 przez MSR 0xC0000080;
   CR0.PG|PE=1. Wymagany zapis MSR jest częścią testu M2 wykonanego przez użytkownika.
5. Far jump do selector 0x18, GDT code64 L=1, D=0. Explicit `mov ebp,ebp`
   zeruje górną połowę RBP przed adresowaniem w 64 bitach. Załadowanie IDT64.
6. Nowy RSP poniżej 4 GiB, alignment 16 i 32 B shadow space. CALL do entry
   payloadu z RCX=HANDOFF, ABI Microsoft X64. Payload ma `ms_abi`.
7. Bezpośredni bounded zapis pikseli do odziedziczonego framebufferu,
   napis `STAGE 2 X64 OK`, następnie CLI/HLT bez powrotu.

Założenie IA32 UEFI: flat protected mode i identity mapped physical addresses
przed SetVirtualAddressMap. EFER.LMA już ustawione jest odrzucane. Nie robimy
thunków do IA32. Nie ma dostępu do GPIO/I2C/MMIO register ani resetów
kontrolerów. Zapisy do framebufferu są celową operacją graficzną testu M2.

IDT celowo zatrzymuje na wyjątku i nie dostarcza diagnozy ekranowej. Istnieje
krótkie okno zmiany formatu IDT przy wejściu w IA32e; CLI nie maskuje NMI,
a SMI może nadal wystąpić. Nie dodano port writes do maskowania NMI ani PIC.
Firmware może pozostawić DMA i procesory pomocnicze aktywne. Ograniczenia
te nie pozwalają na traktowanie tego kodu jako gotowej platformy DXE.

## M3 — zaimplementowany minimalny payload

Oficjalny EDK2 DXE Core i RuntimeDxe są ładowane z niekompresowanego FV.
Własny SEC adapter zamienia rozszerzony HANDOFF na HOB-y, używając
oficjalnych bibliotek payload-entry oraz LoadDxeCore.c. PeCoffLib IA32
ładuje wejście X64 przed EBS. Zachowujemy stos/CR3 transition; DXE
alokuje wyłącznie we własnej arenie 64 MiB. Nowa ConfigurationTable X64
publikuje zachowane wskaźniki ACPI/SMBIOS. Szczegóły: [M3.md](M3.md).

Nie ma thunków do stockowego firmware, BDS ani pełnego zestawu usług
runtime. SMM, AP i DMA pozostają zależne od stockowego firmware.
Nie dodano zapisów SPI ani trwałych variables.

## Dalsze etapy — projekt

M4: zaimplementowany adapter GOP nad odziedziczonym graphics HOB,
bez wiązania PCI/zmiany BAR. Oficjalna FrameBufferBltLib obsługuje
konwersje pikseli; kopiowanie video jest overlap-safe. Jeden tryb,
QueryMode, SetMode i cztery Blt. [Implementacja i testy](M4.md).
QEMU i tablet: sukces (IMG_0429).

M5: użyć PciHostBridge/PciBus, XhciDxe, UsbBusDxe, UsbMassStorageDxe,
DiskIo/Partition/FAT i BDS z EDK2. Najpierw USB; kontrolery eMMC w ACPI mode
wymagają dodatkowych danych platformowych. M6 i Windows dopiero po
ręcznie potwierdzonym M5. ACPI overlays dla Win7 są przyszłym rozszerzeniem,
bez jakiegokolwiek patchowania w bieżącym PoC.

## Aktualizacja M3 — 2026-10-08

Minimalny EDK2 X64 jest zbudowany i sprawdzony w QEMU IA32 OVMF.
Opis implementacji, zakres testów i instrukcja testu tabletu:
[M3.md](M3.md).

## Powłoka X64 — etap dodatkowy

Oficjalny ShellPkg jest uruchamiany przez EDK2 LoadImage/StartImage
z read-only RAM FAT. Konsola korzysta z GraphicsConsole/HII nad GOP M4.
Zmienne są emulowane w RAM. To nie potwierdza natywnego USB ani M5.
[Opis i test](SHELL.md).
