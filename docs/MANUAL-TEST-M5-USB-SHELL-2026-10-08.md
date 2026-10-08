# Fizyczna próba USB i ręcznego Shell — 2026-10-08

IMG_0441–0444 to pierwsze uruchomienie, IMG_0445 według użytkownika to drugie.
Na obu widoczne są USB FILESYSTEM READY, USB SHELL KEYBOARD READY,
przeglądanie FS1 i ręczne polecenie BOOTX64.efi. Potwierdzono działanie
natywnego USB FAT, interaktywnej klawiatury i ręcznego wyboru pliku w Shell.
Nie zmierzono czasu startu. Hash uruchomionego loadera nie jest widoczny.

Użytkownik identyfikuje BOOTX64 jako Ventoy; listing pokazuje 1927600 B.
Program wypisuje out of memory. W drugim uruchomieniu kończy komunikatem
Aborted. Press any key to exit. W pierwszym widoczne są również zniekształcone
teksty i nazwy GRUB/ELF. Nie ustalono przyczyny tego zniekształcenia.
Nie potwierdzono menu Ventoy ani uruchomienia systemu operacyjnego.

Nasze EDK2 udostępnia tylko arenę 64 MiB; reszta stock mapy pozostaje reserved.
To ograniczenie jest kandydatem na przyczynę, ale nie zmierzono pozostałej
wolnej pamięci ani wymagań konkretnego pliku użytkownika. Aktualne źródło
Ventoy GRUB kern/efi/mm.c buduje heap z części pamięci conventional i zawiera
własne reguły rezerwacji. Nie jest to dowód zgodności z wersją na pendrive.
Potrzebny właściwy BOOTX64.EFI do analizy i odtworzenia problemu przed zmianą
polityki pamięci. Nie publikowano kolejnego loadera tylko na podstawie OOM.
Hashe zdjęć i szczegóły w sąsiednim JSON.
