# Ręczny test M4 — zdjęcie otrzymane 2026-10-08

Użytkownik przesłał IMG_0429.HEIC. Widoczne są wszystkie sześć zielonych
komunikatów końcowego testu GOP X64 oraz trzy paski RGB w prawidłowej
kolejności: czerwony, zielony, niebieski. Nazwa i SHA256 oryginału są
w sąsiednim JSON; zdjęcia nie kopiowano do Git.

W obecnym źródle te napisy powstają przez GOP.Blt po testach M3,
QueryMode/SetMode, wszystkich czterech operacji Blt, odczytu kolorów,
nakładania prostokątów i odrzucania błędnych zakresów. Test paddingu
wykonuje się, gdy stride jest większe od width. Zdjęcie potwierdza
osiągnięcie końcowego punktu testu na tablecie i widoczne kolory.
Nie odczytujemy zasłoniętych/niewidocznych parametrów z wcześniejszych zdjęć.

**M4 potwierdzone; bramka do M5 spełniona.** Nie jest to dowód działania
USB/storage, BOOTX64.EFI ani Windows. Dokładny hash uruchomionego EFI
nie jest widoczny na zdjęciu; bieżący hash lokalnego artefaktu jest wyłącznie
odniesieniem. Nie potwierdzono stabilności AP/DMA/SMM ani powtarzalności
cold/warm boot. Agent nie wykonywał testu fizycznego.
