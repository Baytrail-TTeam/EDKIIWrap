# Ręczny test M3 — zdjęcie otrzymane 2026-10-08

Test wykonał użytkownik na tablecie. Dowód: `IMG_0428.HEIC`;
ścieżka i SHA256 oryginału w sąsiednim JSON. Zdjęcia nie kopiowano do Git.

Widoczne cztery białe etapy wejścia X64, przygotowania HOB-ów,
ładowania i wejścia do DXE Core oraz trzy zielone komunikaty:

- `M3 EDK2 DXE OK`
- `BOOT SERVICES TEST OK`
- `ACPI SMBIOS TABLES OK`

W obecnym kodzie probe rysuje je dopiero po zakończeniu testów nowej
SystemTable/BootServices, CRC32, alokacji stron i pool, instalacji/odnalezienia/
usunięcia protokołu, mapy pamięci ograniczonej do areny oraz ponownej
publikacji i sprawdzenia dostępnych wskaźników ACPI/SMBIOS.
Zdjęcie potwierdza osiągnięcie tego punktu wykonania i zachowanie obrazu.

Odczytane wartości: rozmiar mapy wyświetlonej przed final capture `0x7e0`
(2016 B), HANDOFF `0x7b8eb000`, flags `3` (ACPI i SMBIOS obecne), arena
`0x72000000`, FV `0x77972000`, entry point `0x7b9de32c`.
Część wcześniejszych danych collectora zasłaniają paski diagnostyczne.
Nie odtwarzamy zasłoniętych adresów ACPI/SMBIOS z wcześniejszych testów.

**M3 potwierdzone na tablecie; bramka do rozpoczęcia M4 spełniona.**
Nie jest to jeszcze test protokołu GOP X64, USB/storage, BOOTX64.EFI ani
Windows. Nie sprawdzono niezależnie rejestrów CPU, stabilności AP/DMA/SMM,
powtarzalności cold/warm boot ani czasu pozostawania CPU w HLT.
Hash uruchomionego EFI nie jest widoczny na zdjęciu; hash bieżącego lokalnego
artefaktu w JSON jest odniesieniem, nie dowodem tożsamości uruchomionych bajtów.
