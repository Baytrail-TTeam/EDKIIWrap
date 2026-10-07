# Badania źródeł — 2026-10-07

## Lokalne źródła read-only

`../baytrail-device-development` HEAD b78d5b101d4b8138ed85e126a47bc94765c3c71d;
`../baytrail-firmware` HEAD 68ff88fadb52b9afd59ffa390b9b6de5a206303c.
Odczytano AGENTS.md, README, profil Vi10, roadmap, raporty legacy,
analizę ACPI 0xA5, wyniki USB/XSTATE i metadane katalogu firmware.
Hashe dokumentów źródłowych: `docs/LOCAL-SOURCES.json`; nie skopiowano dumpów.

- `legacy/reports/STATUS.md` i `REPORT.md` identyfikują ilife S165/BYT-PF02,
  Atom Z3736F family 6/model 55/stepping 8; CPUID 0x30678 był wnioskiem
  z pól CPU, nie wcześniejszym pomiarem CPUID. Około 2 GiB RAM, UEFI IA32.
- GPU 8086:0f31 rev 0f, BAR0 0x90000000/4 MiB i BAR2
  0x80000000/256 MiB są historycznymi danymi S165. Nie wpisano ich do loadera.
- Aktywny panel opisano jako LTL106AL01/eDP DP-C, 1366×768, 18 bpp
  panelu. To nie oznacza 18-bitowego formatu framebufferu GOP.
  Fotografowany pitch 1376 należy pobrać dynamicznie. W stock VBT są bloki
  MIPI config/sequence v2, ale brak dowodu aktywnego MIPI w tym capture.
- Stock VBT: wersja 100, declared 4042 B, BDB 191; historyczny VBIOS
  Intel 3652 miał BDB155 i nie jest GOP X64. Stock GOP executable/version
  pozostawały niewyekstrahowane; nie znaleziono zweryfikowanego stock SPI dump.
- `boards/chuwi/vi10/device.json` nadal ma soc/firmware_arch/board_revision
  null. ACPI pod tym folderem ma manifest/DMI S165: nie utożsamiamy modeli.
- `docs/ACPI-A5-0x11-ANALYSIS.md`: stock RSDP revision 2, prawidłowe
  RSDT/XSDT, FADT revision 5, FACS, DSDT i dziewięć statycznych SSDT,
  MADT/MCFG/HPET. Historyczne 0x7CCF6014/MCFG 0xE0000000 nie są stałymi
  projektu. SMBIOS legacy 2.7 z poprawnymi checksums.
- SeaBIOS USB/XSTATE zidentyfikował endpoint0 transaction error podczas
  pobierania config descriptor. Istniejąca ścieżka Legacy i jej patche
  pozostały nietknięte. Późniejsze zdjęcie Win7 ACPI stop nie dowodzi
  poprawności tabel w przyszłej ścieżce X64 UEFI ani ukończonej instalacji.
- eMMC i część SDIO/I2C/GPIO/PMIC są zależne od platformy/ACPI. Widoczny
  obraz nie dowodzi gotowości storage czy istnienia sterowników Windows x64.

## EDK2 / payload

Oficjalny [UefiPayloadPkg](https://www.tianocore.org/tianocore-wiki.github.io/platforms-packages/core-packages/uefi_payload_pkg.html)
przejmuje środowisko po bootloaderze; zastąpił stare CorebootModule/PayloadPkg.
Pobrano tag `edk2-stable202608`, commit
2970e5699ba6267f3384ffab20f96647578aebc8 wraz z wybranymi submodules.
Pełne commity pozostałych drzew zapisano w sources.lock.

Analizowane konkretne wejścia:

- [UniversalPayloadEntry.c](https://github.com/tianocore/edk2/blob/2970e5699ba6267f3384ffab20f96647578aebc8/UefiPayloadPkg/UefiPayloadEntry/UniversalPayloadEntry.c):
  BuildHobs bierze BootloaderParameter jako listę HOB zaczynającą się od
  PHIT, wybiera/potwierdza RAM i szuka UniversalPayloadExtraData z FV.
  Nie przyjmuje naszego HANDOFF bez adaptera. Format ELF/FIT wymaga loadera,
  poprawnego położenia sekcji i danych extra, a nie zwykłego wykonania .fd.
- [Ia32/DxeLoadFunc.c](https://github.com/tianocore/edk2/blob/2970e5699ba6267f3384ffab20f96647578aebc8/UefiPayloadPkg/UefiPayloadEntry/Ia32/DxeLoadFunc.c):
  przy PcdDxeIplSwitchToLongMode tworzy GDT, identity page tables, IDT64,
  stos i wywołuje AsmEnablePaging64 z HobList. To gotowy model mieszanej
  architektury, który warto wykorzystać w M3 po naszym testowym M2.
- [EnablePaging64.nasm](https://github.com/tianocore/edk2/blob/2970e5699ba6267f3384ffab20f96647578aebc8/MdePkg/Library/BaseLib/Ia32/EnablePaging64.nasm):
  CLI → CR4.PAE → EFER.LME → CR0.PG → retf → stos/ABI X64.
  Nasz blob jest oryginalnym kodem o tej samej wymaganej kolejności,
  z własnym GDT, wyłączeniem starego paging i świadomymi ograniczeniami IDT.
- [BuildAndIntegrationInstructions](https://github.com/tianocore/edk2/blob/2970e5699ba6267f3384ffab20f96647578aebc8/UefiPayloadPkg/BuildAndIntegrationInstructions.txt):
  opisuje pure X64 oraz IA32 entry/X64 DXE. Przygotowanie całego DXE
  pozostaje M3; obecny build kompiluje tylko nagłówkową aplikację IA32 i
  minimalny payload własny. Nie przedstawiamy go jako kompletnego EDK2 UEFI.

[Intel SDM](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html),
Vol.3A, rozdziały IA-32e paging i Mode Switching (9.8.5) są podstawą
sekwencji PG=0, PAE, CR3/PML4, LME, PG i far jump do CS.L=1/CS.D=0.
Zgodność sekwencji została sprawdzona źródłowo; brak uruchomienia CPU testu.

DUET nie jest obecnym pakietem w pobranym drzewie. Historyczny DUET
był drogą bootowania UEFI na Legacy BIOS; stock IA32 UEFI już wykonał
inicjalizację sprzętu. Portowanie starego DUET nie daje gotowego kontraktu
z obecnym firmware. OVMF jest platformą VM/QEMU: użyteczne do weryfikacji
transition, lecz QemuVideo/Virtio/PCI topology nie odwzorowują tabletu.

## Coreboot i Chromebooki Bay Trail

Pobrano oficjalny coreboot i MrChromebox edk2 oraz blobs; wszystko lokalnie
w ignored third_party. Nie uruchomiono firmware utility ani aktualizatorów.
[Lista wspieranych urządzeń MrChromebox](https://docs.mrchromebox.tech/docs/supported-devices.html)
zawiera Chromebooki Bay Trail, nie stanowi portu dla tego tabletu.
[Opis Full ROM](https://docs.mrchromebox.tech/docs/firmware/types.html) potwierdza
model coreboot + payload UEFI jako zamiennik firmware dla wspieranych płyt.
Windows wymaga odpowiedniego board support i sterowników; nie wynika z
samej zgodności rodziny CPU. [Issue #967](https://github.com/MrChromebox/firmware/issues/967)
opisuje również regresję Windows PE na Haswell/Broadwell/Baytrail, więc
„coreboot → EDK2 → Windows” nie oznacza każdej wersji firmware i każdej płyty.

Istotne pliki w przypiętym coreboot:

- `src/soc/intel/baytrail/`: DRAM/refcode, grafika, stolen memory, ACPI/FADT,
  PCI north/south cluster, xhci/ehci, SCC/eMMC/SD/SATA i SMM.
- `src/mainboard/google/rambi/`: board-specific Chromebook configuration.
  Dane EC/PMIC/panelu tej płyty nie są konfiguracją S165/Vi10.
- `src/arch/x86/boot.c`: arch_prog_run przekazuje payloadowi entry/arg;
  także ramstage X64 schodzi do protected mode, bo standardowy x86 payload
  oczekuje takiego wejścia. Long mode EDK2 jest więc własną decyzją payloadu.
- `src/cpu/x86/64bit/entry64.inc`, `mode_switch2.S`: istniejące coreboot
  mechanizmy wejścia w long mode; nie są adapterem IA32 EFI BootServices.
- `src/lib/coreboot_table.c`, `src/arch/x86/tables.c`: coreboot przekazuje
  memory/framebuffer/ACPI/SMBIOS przez coreboot tables/CBMEM.
- `payloads/external/edk2/Makefile`: BOOTLOADER=COREBOOT, obsługa
  UEFIPAYLOAD.fd/UniversalPayload.fit i opcjonalnego zewnętrznego GOP.

Classic UefiPayloadEntry + CbParseLib konwertują coreboot tables do
resource/graphics/ACPI/SMBIOS HOB. To różni się od UniversalPayloadEntry,
któremu bootloader przekazuje już przygotowane HOB-y. HOB nie jest ogólnym
wskaźnikiem do IA32 EFI ConfigurationTable.

EDK2 DSC zawiera PCI host bridge/bus, XhciDxe, UsbBus, UsbMassStorage,
Disk/Partition/FAT i GraphicsOutputDxe. Nie używa ich stockowych IA32
odpowiedników po przejściu. Driver USB musi uzyskać własne controller state,
a PCI resources muszą respektować framebuffer i firmware reservations.
MrChromebox ma również SdhciNonPciDxe dla kontrolerów SCC w ACPI mode;
bez platform registration nie rozwiązuje automatycznie eMMC tabletu.

## GOP X64 — znaleziony donor

[Konkretny blob Bay Trail](https://github.com/MrChromebox/blobs/blob/a8c9e312c44e108f3c6cfd51953439d353a7a461/soc/intel/byt/IntelGopDriver.efi)
ma PE32+ Machine 8664, subsystem 11 (EFI Boot Service Driver), 75168 B.
String identyfikuje Valley View. Wersja 7.2.1013 pochodzi z versions.txt,
nie z przypuszczenia na podstawie daty kompilacji. Disassembly Supported()
porównuje Intel vendor i tabelę czterech device IDs pod RVA 0x33c0:
8086:0f30, 0f31, 0f32, 0f33. To dowód deklarowanej obsługi ID, nie test GPU.

SHA256 GOP: `0a9c10dcecf861b9c0df2b15f5d08c0426aef5d1e988a1b220f4650e600d33fd`.
Companion `soc/intel/byt/vbt.bin`: VBT100/BDB191, declared 3892 B,
BDB3844 B, checksum0. Ten VBT jest oddzielnym plikiem i ma inne długości
niż stock S165. Nie zastępujemy nim stockowego panel data.
Pełne dane i SHA256 VBT: `GOP-DONOR.json`, skrypt `scripts/analyze-gop.py`.

Dalsze donor families z żądania (Bay Trail D/I/M/T, mini-PC i industrial):
obecny katalog zawiera HPC7SCP4/Jetway, Intel FSP VBIOS i PIPO X9 VBT,
ale nie dowodzi istnienia X64 GOP w tych lokalnych wpisach. PIPO X9S jest
Cherry Trail, więc nie jest ValleyView donor. Oficjalny
[Intel Compute Stick FCBYT10H](https://www.intel.com/content/www/us/en/download/18173/28922/bios-update-for-compute-stick-fcbyt10h.html)
jest dodatkowym tropem; nie pobrano/nie wyekstrahowano jego GOP i nie
przypisujemy mu wersji, PE machine ani zgodności. Ponieważ potwierdzony
X64 ValleyView donor został już znaleziony w blobs, dalsze wydobywanie
nie jest zależnością pierwszego PoC.

Gotowy framebuffer wrapper EDK2 istnieje w
`UefiPayloadPkg/GraphicsOutputDxe/GraphicsOutput.c`. Używa graphics HOB,
FrameBufferBltLib i PCI binding; nie potrzebuje natywnego Intel GOP do
samej prezentacji odziedziczonego trybu. M4 zacznie od tego kodu.
