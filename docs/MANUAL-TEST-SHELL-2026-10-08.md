# Ręczny test Shell X64 — zdjęcie otrzymane 2026-10-08

Użytkownik przesłał IMG_0430.HEIC. Widoczne są banner oficjalnego Shell
v2.2, identyfikacja Baytrail EDK2 X64, mapowanie FS0:, wykonanie echo -off,
ponowny wynik ver/map, SHELL_X64_RAM_OK, prompt Shell> oraz zielony
SHELL X64 SCRIPT OK. Nazwę i SHA256 oryginału zapisano w sąsiednim JSON;
zdjęcia nie kopiowano do Git.

**Uruchomienie powłoki X64 i skryptu z RAM FAT na tablecie potwierdzone.**
W bieżącym źródle końcowy marker powstaje po obserwacji tekstu skryptu
na ConOut i wejściu w idle wait. Następnie program celowo kończy w CLI/HLT.
Widoczne ostrzeżenie o SimpleTextInputEx odpowiada brakowi obsługi klawiatury.
Zdjęcie nie dowodzi interaktywnego wejścia ani działania fizycznego USB.

Pełne M5 pozostaje nieukończone: potrzebne są natywne sterowniki PCI/USB,
klawiatury i odczyt BOOTX64.EFI z fizycznego pendrive'a. Nie testowano Windows.
Hash uruchomionego EFI nie jest widoczny; lokalny hash jest tylko odniesieniem.
Agent nie wykonywał testu fizycznego. Nie potwierdzono powtarzalności bootu.
