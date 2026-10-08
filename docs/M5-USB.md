# M5 — natywne USB X64, pakiet testowy 2026-10-08

Zbudowany `BAYTRAIL-USB-IA32.EFI` zachowuje sprawdzony collector, transition,
SEC/DXE i GOP. Nowy wariant przejmuje wyłącznie xHCI, podłącza oficjalne
XhciDxe, UsbBusDxe, UsbKbDxe, UsbMassStorageDxe, DiskIoDxe, PartitionDxe
oraz FatDxe. Sterowniki upstream pozostają bez zmian.

**To kandydat do ręcznego testu M5. Wynik QEMU nie zamyka bramki M5 na tablecie.**
Powłoka z RAM została wcześniej potwierdzona zdjęciem IMG_0430. Nowy wariant
ma dodatkowo sprawdzić fizyczne USB/storage i rzeczywiste wejście klawiatury.
Nie uruchamia instalatora Windows i nie jest pełną platformą do bootu OS.

## Pliki i ręczny test

Gotowy ZIP: `build/usb/BAYTRAIL-USB-TEST.zip`. W środku są instrukcja oraz:

| Plik | Miejsce na testowym pendrive FAT32 |
|---|---|
| `build/BAYTRAIL-USB-IA32.EFI` | `\EFI\TOOLS\BAYTRAIL-USB-IA32.EFI` |
| `build/usb/BOOTX64.EFI` | `\EFI\BOOT\BOOTX64.EFI` |

Zachowaj ewentualny istniejący BOOTX64.EFI poza ścieżką tego testu.
BOOTIA32.EFI i wcześniejszych loaderów nie trzeba zastępować. Nie zmieniaj
plików na dysku systemowym. Agent przygotowuje pakiet lokalnie; użytkownik
kopiuje pliki i uruchamia test na tablecie.

1. Podłącz pendrive testowy i klawiaturę USB przed startem. Można użyć hubu.
2. Uruchom ręcznie `BAYTRAIL-USB-IA32.EFI` dotychczasową metodą Boot From File
   albo IA32 Shell. Nie uruchamiaj BOOTX64.EFI w stockowej powłoce IA32.
3. Wariant X64 podłącza xHCI i przez 15 s pompuje enumerację USB.
   Z fizycznego USB otwiera `\EFI\BOOT\BOOTX64.EFI` i porównuje cały plik
   z osadzonym wzorcem. Tylko dostarczona aplikacja testowa może wystartować.
4. Testowa aplikacja X64 wypisuje `USB_X64_APP_OK` i wraca do dispatchera.
   Później konsola zostaje wyczyszczona przy starcie Shell z RAM.
   Oczekuj zielonych `USB STORAGE X64 APP OK`, `USB SHELL KEYBOARD READY`
   oraz promptu `Shell>`. Pendrive ma ścieżkę zawierającą `/USB(...)` w mapowaniu;
   numery FS0/FS1 mogą się różnić. Vendor RAM FAT jest oddzielnym woluminem.
5. Wpisz **`echo usb_key_ok`** i naciśnij Enter. Oczekuj wyniku `usb_key_ok`
   oraz zielonego `USB KEYBOARD COMMAND OK`. Zrób zdjęcie całego ekranu.

W odróżnieniu od wcześniejszego pakietu Shell RAM, nowy wariant nie kończy
w CLI/HLT po diagnostyce. Powłoka pozostaje aktywna i odczytuje klawiaturę USB.
Ten etap wymaga klawiatury HID typu boot protocol; nie obejmuje dotyku ani
klawiatury ekranowej. Nie podawaj jeszcze plików Windows do uruchomienia.

## Integracja platformy

Własny ograniczony PciIo publikuje jeden kontroler na bus 0 rozpoznany po
klasie xHCI i ID `8086:0f35` (tablet) lub `1b36:000d` (QEMU). BAR0 pobierany
jest z bieżącej konfiguracji PCI; nie ma adresu BAR z raportu urządzenia.
Wspierane okna to odpowiednio 64 KiB i 16 KiB dla tych dwóch kontrolerów.
Brak reenumeracji/przydziału PCI BAR i brak bindingu eMMC/GPU.
Nie są obsługiwane nieprzypisany BAR, pamięć powyżej 4 GiB ani platforma
publikująca DMAR (IOMMU nie jest obsługiwane).

Przed bindingiem kontrolera sprawdzamy zakresy MMIO i strukturę extended
capabilities. Własne tablice stron ustawiają PCD/PWT dla dużych stron BAR,
z walidacją PAT UC i odmową zmiany cache nad systemową pamięcią RAM.
Występuje flush cache/TLB. Native xHCI może teraz resetować porty i używać DMA;
nie jest to już framebuffer-only PoC.

Jeśli istnieje USB Legacy Support capability, provider prosi stockowy firmware
o własność kontrolera przez OS semaphore, czeka maksymalnie 1 s na zwolnienie
BIOS semaphore i odmawia startu przy timeout. Następnie wyłącza pięć źródeł
SMI lokalnych dla xHCI i potwierdza trzy statusy RW1C, zachowując reserved bits.
Zapisy ownership wykonywane przez oficjalny sterownik pre-OS są normalizowane
przez PciIo.Mem.Write do uzyskanej własności OS. Nie przejmujemy ani nie wyłączamy
SMM całej platformy. Układ pól i sekwencja: [Intel xHCI 1.2b, 4.22.2 i 7.1](https://cdrdv2-public.intel.com/625472/625472_xHCI_Rev1_2b.pdf).

Common DMA buffers są alokowane tylko w własnej, wcześniej zarezerwowanej
arenie 64 MiB. Pozostałe mapowania używają bounce bufferów w tej arenie;
Unmap kopiuje wynik wyłącznie dla BusMasterWrite. Adresy DMA pozostają
poniżej 4 GiB, coherent x86, bez IOMMU. Wbudowane kontrole sprawdzają odmowę
MMIO/config poza zakresem, common mapping spoza areny i kopie DMA w obu
kierunkach. Inne stockowe regiony pamięci pozostają zarezerwowane.

TimerLib odczytuje port PM timer z walidowanego FADT, w tym Extended GAS
SystemIO. Nie programuje LPC ani ACPI i nie zakłada częstotliwości TSC.
Metronome obsługuje oficjalne Boot Services Stall. Timer adapter przekazuje
CoreTimerTick przy Stall, CheckEvent i IdleLoopEvent z elapsed PM ticks. IRQ procesora
pozostają wyłączone. **Jest to timer kooperacyjny do tego PoC, nie pełny
sprzętowy timer PI:** zdarzenia nie postępują podczas długiego kodu, który
nie wykonuje Stall, CheckEvent ani idle. APIC/PIC i pozostałe AP nie są inicjalizowane.

Notify na BlockIo ustawia ReadOnly i zastępuje WriteBlocks odmową
EFI_WRITE_PROTECTED przed połączeniem FAT. Obejmuje dyski USB, partycje
i RAM FAT. Nie ma sterownika eMMC ani zapisów do NVRAM/SPI/BootOrder.
Zmienne emulowane NV nadal istnieją tylko w RAM. RAM FAT zawiera oficjalną
powłokę X64; testowy BOOTX64 z USB jest osobną aplikacją, a nie tą powłoką.

## Build i weryfikacja

```sh
./build-usb.sh
python3 scripts/make-usb-test-disk.py
python3 scripts/test-usb-qemu.py
python3 scripts/test-usb-qemu.py --case hub-mbr
python3 scripts/test-usb-qemu.py --case wrong-app
python3 scripts/test-usb-qemu.py --case no-keyboard
```

Osobny workspace `build/usb/workspace`; dotychczasowy loader Shell nie jest
nadpisywany. Build korzysta z deterministycznego RAM FAT wygenerowanego przez
build-shell.sh, buduje app testową, FV 3 MiB i osadza SEC/FV/transition w EFI IA32.
Weryfikator sprawdza maszyny/subsystemy PE, entry/importy/relokacje, timestamp 0,
FV header checksum, pliki FFS i osadzenie wszystkich wymaganych blobów.
Manifest: `build/usb/manifest.json`; ZIP i jego hashe: `build/usb/package.json`.

QEMU używa IA32 OVMF, q35 TCG, 256 MiB RAM, jednego CPU, osobnego dysku USB
przy qemu-xhci i usb-kbd. Dysk do startu IA32 jest innym urządzeniem i nie jest
publikowany w nowym X64 środowisku. Wyłączona sieć; kończona jest tylko własna VM.

Przypadki pozytywne: bezpośrednie USB z superfloppy FAT16 oraz hub USB z FAT16
w partycji MBR. QMP wpisuje polecenie przez emulowaną klawiaturę HID; marker
klawiatury powstaje dopiero po obserwacji osobnej linii wyniku na ConOut.
Zrzuty ekranu potwierdzają mapowanie USB i rzeczywisty wynik polecenia.
Przypadki negatywne: plik BOOTX64 różny od wzorca i brak klawiatury. Sprawdzamy
właściwy czerwony komunikat i CS64/HLT. Obrazy dysków USB są zapisywalne dla VM,
ale ich SHA256 musi pozostać identyczne po każdym teście.

Raporty QEMU, static checks i rebuild: `docs/M5-USB-QEMU-2026-10-08.json`.
Nie potwierdzono na tablecie: native Intel xHCI, przejęcia ownership/SMM,
klawiatury, urządzeń za fizycznym hubem ani stabilności zimnego/ciepłego bootu.
Nie testowano USB UAS, hotplug nowych klas, GPT/FAT32 w QEMU ani bootu OS.
M5 wymaga jeszcze zdjęcia sukcesu z tabletu i próby wejścia przez klawiaturę.

## Zatrzymania i zdjęcia

`USB PM TIMER UNAVAILABLE` — brak poprawnego PM timer w FADT.
`USB PCI PUBLISH FAILED` — kontroler/BAR/cache/ownership/DMA guard niedostępny;
`USB XHCI DRIVER FAILED` — brak host-controller protocol po ConnectController.
`USB FAT MISSING` albo `USB TEST BOOTX64 NOT FOUND` — nie powstał właściwy
wolumin/plik lub plik różni się od wzorca. `USB KEYBOARD MISSING` — test aplikacji
może być zakończony, ale nie powstał protokół klawiatury USB.
Zrób zdjęcie również przy ASSERT, czarnym ekranie i nieruchomej enumeracji.

## Poprawka zatrzymania enumeracji — 2026-10-08

Użytkownik zgłosił zatrzymanie na „enumerating devices” przez ponad 2 minuty
dla wcześniejszego pakietu. Bez zdjęcia/trace nie ustalono jeszcze dokładnej operacji urządzenia.
Audyt wykazał błąd naszego timera: Busy pozostawało TRUE podczas RestoreTPL,
które dispatchuje callbacki USB. Timeouty tworzone w takich callbackach
nie dostawały ticków mimo wywołań Stall. Dodatkowo UsbBootExecCmdWithRetry
może odpytywać CheckEvent w pętli EFI_NOT_READY bez wykonywania Stall.

Busy jest teraz zwalniane przed dispatchowaniem callbacków. CheckEvent
pompuje timer przed wywołaniem oryginalnej funkcji; CRC Boot Services jest
ponownie obliczane. Wbudowany test tworzy 10 ms timeout wewnątrz callbacku
uruchomionego przez Pump/RestoreTPL i oczekuje przez samo CheckEvent.
Niezależny PM timer przerywa test po 200 ms, jeśli timeout się nie sygnalizuje.
Bez sukcesu tego testu PciIo nie jest publikowane.

Postęp enumeracji wypisuje teraz tick N/150, oczekiwanie na timer oraz osobne
łączenie interfejsów USB i dysków/FAT. USB/FAT UEFI_DRIVER korzystają również
z framebufferowego reportera DEBUG_ERROR/ASSERT. Komunikat „15 s” oznacza
czas głównej pętli; synchroniczne próby sterowników mogą go wydłużyć,
w tym retry gotowości pamięci masowej ograniczony do 60 s przez EDK2.

Dokładny wynik nowego testu fizycznego nadal oczekuje na użytkownika.
Nowy pakiet ma taki sam plik diagnostyczny BOOTX64.EFI, ale nowy loader IA32.
Raport poprawki: M5-USB-TIMER-FIX-2026-10-08.json. Poprzedni raport i hashe
pozostają dowodem wcześniejszej wersji, nie poprawionego loadera.

## Diagnostyka blokady disks/FAT — 2026-10-08

Użytkownik zgłosił zatrzymanie poprawionego wariantu na „connecting disks/FAT”
przy enumerate tick 0/150. Nie ustalono jeszcze przyczyny: komunikat poprzedza
rekurencyjne ConnectController na pierwszym zbiorze BlockIo.

Nowy wariant zachowuje kolejność bindingu sterowników. Opakowuje Supported
oraz Start tylko dla oficjalnych DiskIoDxe, PartitionDxe i FatDxe, rozpoznanych
po GUID pliku FV w LoadedImage. Markery zawierają nazwę sterownika, fazę,
handle i status powrotu. Konsola pokazuje parametry każdego BlockIo:
media ID, wielkość sektora, ostatni LBA i oznaczenie partycji.

Notify BlockIo zachowuje blokadę WriteBlocks i dodatkowo opakowuje ReadBlocks.
Na ekranie widoczne są numer dysku, ostatni LBA, liczba bajtów i status powrotu.
Hook wywołuje oryginalne ReadBlocks bez zmiany danych. Zagnieżdżone odczyty
partycji i dysku fizycznego zachowują wspólny czas trwania odczytu zewnętrznego.
Wariant ma tablicę dla 64 protokołów BlockIo; kolejne są chronione przed zapisem,
ale nie otrzymują read trace. Nie gwarantuje obsługi hotplug/reuse handle.

Podczas aktywnego ReadBlocks Pump sprawdza niezależne elapsed PM ticks.
Po 30 s rysuje czerwony USB DISK READ TIMEOUT, numer dysku i LBA, następnie
zatrzymuje CPU bez rebootu. Watchdog jest kooperacyjny: wymaga osiągania Pump
przez Stall/CheckEvent/idle; nie przerwie ciasnej pętli bez tych wywołań.
Nie wycofuje aktualnego transferu ani nie resetuje kontrolera. Dane i bufory
pozostają zarezerwowane tak jak przy pozostałych zatrzymaniach PoC.

Markery bindingu i odczytów są aktywne podczas uruchamiania USB. Po sukcesie
testu dysku i wykryciu klawiatury znikają z dalszych wywołań, aby nie nadpisywać
konsoli Shell. Watchdog odczytu pozostaje aktywny również w Shell.

Potrzebne jest zdjęcie całego ekranu nowej próby, szczególnie wierszy 17–20:
np. PARTITION START, FAT SUPPORTED, DISK READ LBA i READ RETURN.
Ta wersja jest diagnostyczna; nie deklarujemy naprawy fizycznej blokady.
BOOTX64.EFI pozostaje bez zmian. Raport QEMU tej wersji:
M5-USB-DISK-TRACE-2026-10-08.json.

## Odczyt sektora 2048 — zdjęcia IMG_0432–0435

Zdjęcia otrzymane 2026-10-08 potwierdzają wykonanie kooperacyjnego watchdogu
na tablecie: USB DISK READ TIMEOUT, DISK 0 LBA 2048, odczyt 512 B.
Wcześniej widoczny jest udany odczyt LBA 0. Pozostający READ RETURN 0
pochodzi z poprzedniego wywołania, ponieważ wcześniejszy trace nie zerował
wiersza powrotu przed rozpoczęciem kolejnego odczytu. Część wcześniejszych
zdjęć uchwyciła markery w trakcie przerysowywania i nie pozwala na odczyt
jednoznacznej pary Supported/Start–return. Hashe zdjęć i obserwacje:
MANUAL-TEST-M5-USB-2026-10-08.json. Hash EFI nie jest widoczny na zdjęciach.

Kolejna wersja pokazuje PENDING przed wywołaniem ReadBlocks i bindingu.
Notify UsbIo opakowuje tylko Control/Bulk urządzeń klasy mass storage (08),
bez zmian danych, parametrów i statusów oryginalnych wywołań. Wiersze 11–16
pokazują control request/status, ostatni REQUEST SENSE, numer/opcode/tag CBW,
endpoint/liczbę bajtów/timeout bulk, return USB i CSW status/tag.
Tablica obejmuje do 64 UsbIo; brak gwarancji hotplug/reuse pointer jak przy
BlockIo. Trace nie obejmuje wewnętrznych ringów xHCI. Diagnostyka wyłącza się
przed Shell. Nadal obowiązuje limit 30 s dla całego zewnętrznego ReadBlocks;
może on uciąć legalne retry media-not-ready, które EDK2 ogranicza do 60 s.
Nie deklarujemy naprawy transportu. Potrzebne zdjęcie końcowe z wierszami 11–20.
Raport QEMU: M5-USB-TRANSPORT-TRACE-2026-10-08.json.

Watchdog ponownie rysuje ostatnie markery transportu przed zatrzymaniem,
ponieważ przewijanie konsoli GOP może naruszać wcześniejsze overlaye.
CSW i sense oznaczają ostatnią odebraną odpowiedź; BULK PENDING oznacza
aktualnie niewrócony transfer.

## Próba transportu — kolejne zdjęcie z 2026-10-08

Widać READ(10), opcode 28, tag 30D, endpoint IN 82, oczekiwane 512 B,
timeout pojedynczego bulk 5000 ms i BULK PENDING. Odczyt BlockIo dotyczy
LBA 31457279. Ostatni CSW ma status 0 i tag 30C, zatem nie odpowiada
bieżącemu tagowi 30D. Sense key 6 / ASC 28 / ASCQ 00 jest ostatnią zachowaną
odpowiedzią, nie dowodem błędu bieżącego polecenia. W źródle UsbMassBoot
oznacza unit attention / media change. Licznik 781 jest sumą CBW od
publikacji UsbIo, nie liczbą ponowień tego sektora.

PartitionDxe/Gpt.c sprawdza backup przy LastBlock, jeśli nagłówek primary
nie przechodzi walidacji. Adres 31457279 jest zgodny z końcem urządzenia
15 GiB, ale zdjęcie nie pokazuje LastBlock; pochodzenie odczytu pozostaje
hipotezą do sprawdzenia. Bezpośrednio potwierdzono tylko aktywny READ(10)
i brak jego zakończenia przed naszym limitem 30 s.

Nowa próba wydłuża zewnętrzny watchdog do 120 s, aby nie przerywać od razu
60-sekundowej ścieżki retry EDK2. Po 30/60/90 s pokazuje pomarańczowy READ WAIT
i odświeża trace, nadal pozwalając oryginalnemu odczytowi działać. Po 120 s
zatrzymuje CPU i pokazuje również LastBlock. Limit nadal może uciąć kilka
kolejnych pełnych cykli retry/reset; nie jest dowodem uszkodzenia pendrive'a
ani naprawą xHCI/GPT. Dysk nie jest modyfikowany. Nowy raport QEMU:
M5-USB-LONG-READ-2026-10-08.json.
