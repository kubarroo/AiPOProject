# Inteligentne skalowanie obrazów

## Opis

Inteligentne skalowanie obrazów (Content-Aware Image Resizing) przy użyciu algorytmu Seam Carving.

## Skład zespołu

- Krzysztof Jabłoński
- Jakub Szydełko
- Marcin Zepp

## Idea rozwiązania i algorytmy

Celem projektu jest stworzenie aplikacji desktopowej pozwalającej na zmianę rozmiaru obrazu bez zniekształcania kluczowych obiektów, poprzez usuwanie pikseli o najmniejszym znaczeniu dla odbiorcy.

### MVP

Aplikacja ładuje obraz poprzez mechanizm **drag-and-drop**. Wraz ze zmianą rozmiaru ramki obrazu załadowany obraz jest na bieżąco skalowany (przycinany/rozszerzany) z wykorzystaniem algorytmu Seam Carving, dopasowując się do aktualnie dostępnego miejsca.

### Kluczowe elementy rozwiązania

#### Funkcja energii

Obliczanie mapy energii pikseli za pomocą operatora Sobela, aby zidentyfikować krawędzie i obszary o dużej zmienności (obszary ważne).

#### Programowanie dynamiczne

Wykorzystanie programowania dynamicznego do znalezienia ciągłej ścieżki pikseli (szwu) z góry na dół (lub od lewej do prawej) o najmniejszej skumulowanej energii.

#### Usuwanie/Dodawanie szwów

Modyfikacja macierzy obrazu polegająca na usunięciu znalezionego szwu lub duplikacji jego pikseli w celu powiększenia obrazu.

## Technologie

- **C++20** – główny język programowania.
- **OpenMP** – biblioteka używana do zrównoleglenia obliczeń (np. jednoczesne liczenie mapy energii dla różnych wierszy obrazu). Pozwala na znaczne przyspieszenie działania algorytmu Seam Carving.
- **OpenCV** – wykorzystywane do wstępnego przetwarzania obrazu (wczytywanie i zapisywanie plików w różnych formatach, takich jak PNG/JPG).
- **SDL3** – niskopoziomowa biblioteka deweloperska używana do stworzenia okna aplikacji, zarządzania kontekstem graficznym oraz obsługi zdarzeń. Odpowiada za bezpośrednie, wydajne renderowanie przetworzonych klatek obrazu na ekranie.
- **Dear ImGui** – lekka biblioteka graficznego interfejsu użytkownika (Immediate Mode GUI). Służy do stworzenia panelu kontrolnego aplikacji.
- **CMake** – system automatyzacji procesu budowania projektu. Gwarantuje niezależność od platformy i ułatwia automatyczne linkowanie zewnętrznych bibliotek na systemach Windows, Linux oraz macOS.


## Plan pracy

1. Przygotowanie repozytorium i struktury projektu. ✔️
2. Przygotowanie narzędzi do budowania projektu. ✔️
3. Dodanie obsługi zdarzenia drag-and-drop dla plików graficznych. ✔️
4. Implementacja funkcji obliczającej mapę energii obrazu.
5. Implementacja algorytmu wyszukiwania optymalnego szwu pionowego i poziomego.
6. Implementacja mechanizmu usuwania wskazanych szwów z macierzy obrazu.
7. Stworzenie interfejsu graficznego (GUI).
8. Integracja algorytmu z logiką interfejsu – podpięcie algorytmu pod zdarzenia zmiany rozdzielczości.
9. Ewentualna optymalizacja algorytmów.
10. Ręczne testowanie aplikacji.
11. Opracowanie szczegółowej dokumentacji.

## Podział zadań

`To-Do`


## Instrukcja działania

### Instalacja

`To-Do`

### Uruchomienie

`To-Do`

## Instrukcja użytkowania

`To-Do`

## Przykład działania

`To-Do`

## Co nie działa

`To-Do`
