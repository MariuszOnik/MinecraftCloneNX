# Voxel Game — plan wykonawczy dla Codexa w VS Code

## 1. Cel projektu

Tworzymy własną grę voxelową w C++17 na raylib. Świat z chunków ma być fundamentem pełnej gry, a nie wyłącznie demonstracją renderowania bloków.

Projekt ma docelowo obsługiwać:

- proceduralny świat z chunków;
- kopanie, budowanie, kolizje i zapis zmian;
- półprzezroczyste okna, wodę i inne warstwy materiałów;
- voxelowe postacie i zwierzęta złożone z hierarchicznych, sztywnych części;
- własny edytor modeli i animacji voxelowych;
- import ruchu z FBX, BVH i GLB przez narzędzie wykorzystujące Blendera;
- logikę gry, NPC, zwierzęta, questy i interakcje pisaną w Lua 5.4;
- wersję PC oraz Nintendo Switch homebrew przez raylib-nx i devkitA64;
- lokalne budowanie w VS Code oraz zdalne budowanie przez GitHub Actions.

Gra nie ma być kopią zawartości Minecrafta. Korzystamy z dobrych technik silników voxelowych, ale tworzymy własne mechaniki, modele, materiały i oprawę.

## 2. Twarde decyzje architektoniczne

1. **C++ jest rdzeniem silnika.** Zarządza chunkami, meshowaniem, renderingiem, fizyką, encjami, nawigacją, animacją i zapisem.
2. **Lua jest warstwą gameplayu.** Obsługuje zachowania, eventy, interakcje, questy i scenariusze. Nie wykonuje meshowania ani bezpośrednich wywołań raylib.
3. **Raylib jest backendem renderowania i platformy.** Kod gry nie może rozsypywać wywołań raylib po warstwie logiki.
4. **PC-first, Switch-always.** Najczęściej pracujemy i testujemy na PC, ale kod ma się kompilować na Switchu po każdym ukończonym kamieniu milowym.
5. **Brak rysowania każdego voxela osobno.** Nie używamy `DrawCube()` dla świata ani gotowej postaci. Woksele są zamieniane na meshe.
6. **Postacie nie używają klasycznego deformowanego skinningu jako głównego systemu.** Model składa się z voxelowych części połączonych hierarchią i pivotami.
7. **FBX/BVH/GLB są formatami źródłowymi narzędzi desktopowych.** Gra i Switch wczytują wyłącznie nasze lekkie formaty runtime.
8. **Przezroczystość jest osobnym pipeline'em.** Opaque, cutout i transparent nigdy nie trafiają do jednego mesha ani jednego przebiegu renderowania.
9. **Zależności i toolchain są przypięte do wersji.** Build lokalny i CI mają dawać powtarzalny wynik.
10. **Najpierw mierzymy, potem optymalizujemy.** Debug HUD i liczniki powstają razem z pierwszym mesherem.

## 3. Sposób pracy: komputer, GitHub i telefon

### 3.1 Praca lokalna na komputerze

Codex działa w VS Code i wykonuje pełny cykl:

1. modyfikuje kod lub zasoby;
2. kompiluje lokalną wersję PC;
3. uruchamia testy;
4. uruchamia grę albo odpowiednie narzędzie;
5. sprawdza logi i liczniki;
6. jeżeli lokalny devkitPro jest dostępny, buduje także `.nro`;
7. tworzy mały, opisany commit;
8. wypycha commit na GitHub;
9. sprawdza wynik GitHub Actions.

Lokalna kompilacja jest podstawowym, najszybszym obiegiem. GitHub Actions nie zastępuje zwykłego testowania na komputerze.

### 3.2 Praca z telefonu

Gdy zmiana jest zlecana z telefonu:

1. kod jest zmieniany w repozytorium lub przygotowywany jako commit/PR;
2. zmiana trafia na GitHub;
3. GitHub Actions automatycznie buduje wersję PC i Switch;
4. wynik zawiera gotowy plik `.nro` oraz katalog `romfs`;
5. paczka ZIP jest pobierana na telefon;
6. `.nro` jest testowane w emulatorze Switcha na telefonie;
7. wynik testu, zrzut ekranu i log błędu wracają do kolejnego zadania dla Codexa.

Telefon nie kompiluje gry lokalnie. GitHub jest zdalną maszyną budującą.

### 3.3 Automatyczne buildy

GitHub Actions ma udostępniać:

- `VoxelGame-Windows.zip` — build desktopowy;
- `VoxelGame-Switch.zip` — `voxelgame.nro`, `romfs` i `build-info.txt`;
- raport testów jednostkowych;
- numer commita widoczny w grze i w paczce.

Każdy push i pull request uruchamia testy oraz buildy. Tag, np. `v0.1.0`, tworzy trwały GitHub Release. Workflow ma również obsługiwać ręczne uruchomienie z przycisku.

Build Switch wykorzystuje przypięty obraz `devkitpro/devkita64`, a nie niekontrolowane `latest`.

## 4. Proponowana struktura repozytorium

```text
voxel-game/
├── .github/
│   └── workflows/
│       ├── build-desktop.yml
│       ├── build-switch.yml
│       └── release.yml
├── .vscode/
│   ├── tasks.json
│   └── launch.json
├── cmake/
│   └── toolchains/
│       └── switch.cmake
├── docs/
│   ├── ARCHITECTURE.md
│   ├── ASSET_FORMATS.md
│   ├── LUA_API.md
│   └── TRANSPARENCY.md
├── src/
│   ├── app/
│   ├── core/
│   ├── world/
│   ├── render/
│   ├── entity/
│   ├── animation/
│   ├── scripting/
│   ├── gameplay/
│   └── platform/
├── tools/
│   ├── voxel_editor/
│   ├── asset_compiler/
│   └── blender_retarget/
├── scripts/
│   ├── entities/
│   ├── items/
│   ├── quests/
│   └── tests/
├── assets_source/
│   ├── voxel_models/
│   ├── animations/
│   └── textures/
├── assets_runtime/
│   ├── models/
│   ├── animations/
│   └── textures/
├── tests/
├── CMakeLists.txt
├── CMakePresets.json
├── PLAN.md
└── README.md
```

## 5. Główne moduły silnika

### 5.1 Core i platforma

- `Game` — cykl życia aplikacji i stany gry;
- `Clock` — stały krok logiki i czas renderowania;
- `Input` — wspólne akcje PC/Joy-Con zamiast klawiszy zaszytych w gameplayu;
- `FileSystem` — różnice między PC, `romfs` i zapisem na karcie SD;
- `JobSystem` — generowanie danych i meshy na workerach;
- `Diagnostics` — logi, asercje, liczniki i build ID.

Wywołania GPU zawsze odbywają się na głównym wątku. Worker może stworzyć dane CPU mesha, ale nie może wywoływać uploadu raylib/rlgl.

### 5.2 Świat voxelowy

Podstawową jednostką jest sekcja `16 × 16 × 16`. Sekcje pionowe są grupowane w kolumny świata.

```cpp
using BlockId = uint16_t;

struct ChunkSection {
    BlockId blocks[16 * 16 * 16];
    bool dataDirty;
    bool meshDirty;
};
```

Świat udostępnia jednolite API:

```cpp
BlockId GetBlock(int x, int y, int z);
void SetBlock(int x, int y, int z, BlockId block);
```

Kod gameplayu nie oblicza ręcznie pozycji i indeksu chunka.

### 5.3 Meshing i optymalizacje

Kolejność wdrażania:

1. jeden testowy chunk;
2. usuwanie ścian zasłoniętych przez sąsiada;
3. poprawne sprawdzanie sąsiadów na granicach chunków;
4. greedy meshing;
5. osobne meshe `opaque`, `cutout`, `transparent`;
6. przebudowa tylko sekcji oznaczonych jako dirty;
7. kolejka meshowania z limitem pracy na klatkę;
8. frustum culling;
9. streaming według odległości od gracza;
10. paleta bloków i kompresja danych;
11. opcjonalne dalsze culling/LOD dopiero po profilowaniu.

Klucz łączenia quadów w greedy mesherze zawiera co najmniej:

- BlockId i materiał ściany;
- indeks tekstury dla danego kierunku;
- kierunek normalnej;
- warstwę renderowania;
- poziom światła;
- ambient occlusion;
- parametry przezroczystości.

Greedy meshing nie może rozciągać pojedynczego kafelka tekstury. Atlas otrzyma marginesy, a shader lokalne UV powtarzane wewnątrz wybranego kafelka.

### 5.4 Rendering przezroczystości i okien

Kolejność renderowania:

1. **Opaque:** depth test ON, depth write ON, blending OFF.
2. **Cutout:** depth test ON, depth write ON, piksele alfa poniżej progu są odrzucane.
3. **Transparent:** depth test ON, depth write OFF, blending ON, kolejność od najdalszych do najbliższych.

Transparentne quady są przechowywane oddzielnie. Na początek sortujemy widoczne sekcje i grupy materiałów, a dla jakości wymaganej przez kilka nakładających się szyb możemy aktualizować kolejność indeksów quadów bez przebudowy ich wierzchołków.

Reguły sąsiedztwa:

- szkło–szkło tego samego typu: usuń wewnętrzną ścianę;
- szkło–powietrze: zachowaj ścianę;
- szkło–blok opaque: zastosuj tabelę reguł materiału;
- szkło–woda: oddzielne meshe i jawna kolejność renderowania.

Obsługujemy zarówno pełny szklany blok, jak i cienką szybę/panel. Bloki mogą posiadać kształt inny niż pełny sześcian.

Pierwszy test przezroczystości musi zawierać:

- kilka szyb ustawionych jedna za drugą;
- kolorowe szkło;
- wodę widzianą przez szybę;
- szybę przed i za obiektem opaque;
- ruch kamery dookoła sceny;
- test na PC, emulatorze i fizycznym Switchu, gdy jest dostępny.

### 5.5 Encje i gameplay

Bloki należą do świata. Postacie, zwierzęta, przedmioty i pociski są encjami.

Minimalne komponenty:

- `TransformComponent`;
- `VoxelModelComponent`;
- `AnimatorComponent`;
- `ColliderComponent`;
- `CharacterMotorComponent`;
- `NavigationAgentComponent`;
- `HealthComponent`;
- `ScriptComponent`.

Zaczynamy od lekkiego rejestru encji i pul komponentów. Nie wdrażamy rozbudowanego frameworka ECS bez rzeczywistej potrzeby.

## 6. Voxelowe modele postaci i zwierząt

### 6.1 Budowa modelu

Model jest hierarchią sztywnych części. Każda część posiada:

- nazwę i identyfikator;
- rodzica;
- lokalną pozycję, obrót i skalę;
- pivot;
- własną tablicę wokseli;
- wygenerowany mesh;
- materiał lub paletę materiałów.

Przykład:

```text
root
└── body
    ├── head
    │   └── beak
    ├── wing.L
    ├── wing.R
    ├── leg.L
    └── leg.R
```

Każda część jest meshowana oddzielnie z face cullingiem i greedy meshingiem. Mesh jest przebudowywany tylko po edycji modelu lub wczytaniu assetu. Animacja zmienia wyłącznie macierze części.

### 6.2 Animacja

Klip animacji przechowuje ścieżki transformacji części:

- czas klatki;
- lokalne przesunięcie;
- lokalny obrót jako quaternion;
- opcjonalną skalę;
- interpolację;
- eventy, np. `footstep`, `hit`, `sound`, `spawn_item`.

Pierwsza wersja obsługuje:

- `idle`, `walk`, `run`;
- animacje zapętlone i jednorazowe;
- cross-fade pomiędzy klipami;
- callback zakończenia;
- root motion jako opcję, nie wymóg.

### 6.3 Format assetów

W edytorze używamy czytelnych plików źródłowych:

- `.vxm.json` — model voxelowy;
- `.vxa.json` — animacja.

Asset compiler zamienia je na lekkie, wersjonowane formaty binarne:

- `.vxm` — model runtime;
- `.vxa` — animacja runtime.

Plik runtime zawiera magic, wersję formatu, rozmiary sekcji oraz kontrolę poprawności. Nie zapisujemy surowych struktur C++ przez `fwrite`.

## 7. Edytor voxelowych modeli i animacji

Edytor jest oddzielną aplikacją desktopową korzystającą z tych samych bibliotek renderowania i formatów co gra.

Zakres pierwszej wersji:

- tworzenie i usuwanie wokseli;
- wybór koloru/materiału;
- tworzenie części;
- ustawienie pivota;
- hierarchia rodzic–dziecko;
- widok perspektywiczny i ortograficzny;
- gizmo przesunięcia i obrotu;
- oś czasu;
- klatki kluczowe;
- odtwarzanie i zapętlanie;
- zapis `.vxm.json` i `.vxa.json`;
- uruchomienie asset compilera i podgląd wersji runtime.

Edytor nie musi działać na Switchu.

## 8. Import FBX, BVH i GLB oraz retargeting mocapu

Blender jest zewnętrznym konwerterem uruchamianym z edytora albo linii poleceń. Do gry nie linkujemy Assimp ani SDK FBX.

Pipeline:

```text
FBX / BVH / GLB
        ↓
Blender + skrypt Python
        ↓
mapowanie kości źródłowych
        ↓
standardowy rig humanoid/quadruped/bird
        ↓
próbkowanie, wygładzenie, redukcja klatek
        ↓
.vxa.json
        ↓
poprawki w naszym edytorze
        ↓
.vxa runtime
```

Standardowe rodziny rigów:

- `humanoid_simple`;
- `quadruped_simple`;
- `bird_simple`.

Importer ma:

1. rozpoznawać popularne nazwy kości, w tym konwencję Mixamo;
2. pozwalać na ręczne poprawienie mapowania;
3. wyliczać korektę pozy spoczynkowej;
4. przenosić przede wszystkim lokalne obroty;
5. skalować przesunięcie root według rozmiaru postaci;
6. ignorować lub scalać palce, dodatkowe kręgi i kości pomocnicze;
7. próbkować ruch w ustalonym FPS;
8. wygładzać szum mocapu;
9. redukować klatki z kontrolowanym błędem;
10. umożliwiać ręczne poprawki po imporcie.

## 9. Lua 5.4

Lua jest linkowana statycznie na PC i Switchu. Używamy niewielkich własnych bindingów C/C++, aby identyczne API działało na obu platformach.

Lua otrzymuje moduły wysokiego poziomu:

- `Game`;
- `World`;
- `Entity`;
- `Animation`;
- `Inventory`;
- `Quest`;
- `Audio`;
- `Time`;
- `Random`.

Lua nie otrzymuje surowych wskaźników ani bezpośredniego dostępu do raylib. Encje są przekazywane jako walidowane uchwyty z generacją.

C++ wykonuje:

- kolizje i fizykę;
- pathfinding;
- przesunięcie postaci;
- meshowanie;
- renderowanie;
- streaming;
- zapis niskopoziomowy.

Lua podejmuje decyzje:

- wybór celu;
- zachowanie NPC/zwierzęcia;
- reakcja na event;
- rozpoczęcie animacji;
- interakcja i dialog;
- zmiana stanu questa.

Obsługujemy eventy i coroutines, np. `wait`, `wait_event`, `wait_until_arrived`. Nie wywołujemy ciężkiego `on_update` dla każdej dalekiej encji w każdej klatce.

Stan zapisu Lua jest jawny i ograniczony do serializowalnych typów. Nie próbujemy serializować całej maszyny Lua ani aktywnego stosu coroutine.

## 10. Nawigacja po voxelach

Pathfinding nie działa na wszystkich blokach świata. Dla aktywnych chunków budujemy uproszczoną reprezentację powierzchni możliwych do przejścia.

Uwzględniamy:

- wysokość i szerokość encji;
- wysokość kroku;
- spadek i przepaść;
- wodę i koszt terenu;
- drzwi oraz dynamiczne przeszkody;
- unieważnienie danych po zmianie bloków.

C++ liczy trasę. Lua wybiera cel i zachowanie.

## 11. Zapis świata

Nie zapisujemy całego proceduralnie wygenerowanego świata. Zapis obejmuje:

- seed i wersję generatora;
- ustawienia świata;
- pozycję oraz stan gracza;
- zmienione chunki;
- encje trwałe;
- stany Lua wskazane przez skrypty;
- wersje formatów assetów i save'a.

Zmodyfikowane chunki używają palety i prostej kompresji, początkowo RLE. Zapis ma być odporny na przerwanie: plik tymczasowy, walidacja, a następnie bezpieczna zamiana.

## 12. Testy i diagnostyka

### 12.1 Testy automatyczne

Bez uruchamiania grafiki testujemy:

- indeksowanie voxeli;
- współrzędne world/chunk/local, także dla liczb ujemnych;
- wykrywanie widocznych ścian;
- greedy meshing na znanych układach;
- reguły przezroczystych sąsiadów;
- serializację `.vxm`, `.vxa` i save'a;
- redukcję keyframe'ów;
- uchwyty encji;
- podstawowe bindingi Lua.

### 12.2 Debug HUD

Gra pokazuje opcjonalnie:

- FPS i frame time;
- liczbę aktywnych/widocznych chunków;
- kolejkę generowania i meshowania;
- liczbę quadów i trójkątów według warstwy;
- czas generowania oraz meshowania;
- liczbę aktywnych i animowanych encji;
- liczbę wywołań Lua;
- pamięć świata, meshów i assetów;
- identyfikator buildu.

## 13. Kamienie milowe

### Zaakceptowany priorytet wykonawczy: voxel-first

Po ukończeniu M0 realizujemy najpierw fundament świata voxelowego. Spike dynamicznego mesha z M1 jest wykonywany razem z pierwszą testową sekcją M2, a spike przezroczystości zostaje włączony do etapu osobnych warstw materiałów. Spike Lua i hierarchicznego modelu pozostają wymagane, ale nie blokują budowy podstaw chunków.

Kolejność prac:

1. dynamiczny mesh oraz jedna testowa sekcja `16 × 16 × 16`;
2. rejestr bloków i usuwanie ścian zasłoniętych przez sąsiada;
3. sąsiedzi na granicach sekcji i oznaczanie obu sekcji jako dirty;
4. greedy meshing;
5. osobne warstwy `opaque`, `cutout`, `transparent` i scena przezroczystości;
6. kolejka przebudowy meshów z limitem pracy na klatkę;
7. frustum culling;
8. streaming według odległości od gracza;
9. paleta bloków i kompresja danych;
10. dalsze optymalizacje dopiero po pomiarach.

Każdy zamknięty wycinek przechodzi lokalny build i test PC, build Switch w GitHub Actions oraz test pobranego `.nro` w emulatorze. Nie łączymy całej listy w jeden duży etap.

### M0 — repozytorium i powtarzalny build

Rezultat:

- repo GitHub;
- CMake dla PC;
- build raylib-nx/devkitA64;
- zadania VS Code;
- GitHub Actions dla PC i Switch;
- pusty program tworzący okno na PC oraz poprawne `.nro`.

Warunek odbioru:

- lokalny build PC działa;
- CI publikuje obie paczki;
- `.nro` uruchamia się w emulatorze telefonu;
- build pokazuje hash commita.

### M1 — spike ryzyk technicznych

Tworzymy małe, odseparowane testy:

1. dynamiczny mesh na PC i NX;
2. przezroczyste szyby i woda;
3. Lua wywołująca bezpieczne API C++;
4. hierarchiczny voxelowy model z animacją dwóch części.

Warunek odbioru:

- wszystkie cztery testy przechodzą na PC;
- powstaje `.nro` z tym samym kodem;
- okna są oglądane z wielu stron bez podstawowych błędów depth write.

### M2 — chunk i mesher

Rezultat:

- sekcja `16³`;
- block registry;
- face culling;
- granice sąsiednich chunków;
- greedy meshing;
- osobne warstwy materiałów;
- testy jednostkowe i HUD.

Warunek odbioru:

- pełny sześcian bloków redukuje się do oczekiwanej liczby quadów;
- zmiana bloku na granicy oznacza oba chunki jako dirty;
- nie ma wewnętrznych ścian między identycznymi blokami.

### M3 — świat i gracz

Rezultat:

- wiele chunków;
- generator z seedem;
- streaming;
- kamera FPS;
- AABB gracza, grawitacja i skok;
- voxel raycast DDA;
- niszczenie i stawianie bloków.

Warunek odbioru:

- gracz może przejść, wykopać i postawić bloki w świecie większym niż bieżący promień chunków;
- chunki poprawnie wchodzą i wychodzą z pamięci.

### M4 — pełny pipeline materiałów i okien

Rezultat:

- opaque/cutout/transparent;
- pełne szkło i cienkie szyby;
- sortowanie przezroczystości;
- reguły szkło–szkło i szkło–woda;
- scena testowa „komnata szyb”.

Warunek odbioru:

- scena przechodzi test z każdej strony na PC i w buildzie NX;
- transparentne materiały nie psują depth buffera opaque.

### M5 — voxel model runtime

Rezultat:

- hierarchia części i pivoty;
- meshowanie części;
- `.vxm.json` i `.vxm`;
- renderer modelu;
- co najmniej dwa modele: humanoid i kura.

Warunek odbioru:

- części poruszają się bez przebudowy mesha;
- model ładuje się identycznie na PC i NX.

### M6 — edytor i animacje

Rezultat:

- malowanie voxeli;
- części i hierarchia;
- pivoty;
- timeline oraz keyframe'y;
- `.vxa.json` i `.vxa`;
- odtwarzanie, pętle i cross-fade.

Warunek odbioru:

- w edytorze tworzymy od zera prostą animację chodu kury;
- gra wczytuje ją bez konwersji runtime.

### M7 — importer mocapu

Rezultat:

- Blender Python importer;
- FBX/BVH/GLB;
- mapowanie humanoida;
- korekta rest pose;
- sampling, smoothing i redukcja;
- podgląd oraz poprawki w edytorze.

Warunek odbioru:

- zewnętrzna animacja chodu jest przeniesiona na voxelowego humanoida;
- wynik działa bez Blendera na PC i Switchu.

### M8 — encje i Lua

Rezultat:

- registry encji i komponenty;
- statyczna Lua 5.4;
- API `World`, `Entity`, `Animation`;
- eventy i coroutines;
- serializowalny stan skryptu;
- prosty navigation agent.

Warunek odbioru:

- zachowanie kury jest w całości sterowane przez `chicken.lua`;
- zmiana zachowania nie wymaga rekompilacji C++ na PC;
- paczka NX zawiera i wczytuje ten sam skrypt.

### M9 — pierwszy vertical slice

Scena zawiera:

- proceduralną łąkę;
- streamowane chunki;
- kopanie i budowanie;
- dom z poprawnie renderowanymi oknami;
- animowaną kurę sterowaną Lua;
- chodzenie, dziobanie, reakcję na gracza i omijanie dziury;
- zapis zmian świata i stanu zwierzęcia.

To jest pierwsza wersja, którą uznajemy za zalążek gry, a nie demo technologiczne.

### M10 — profilowanie i Switch

Rezultat:

- profilowanie CPU, GPU i pamięci;
- budżety chunków i encji;
- rzadsze aktualizacje dalekich zwierząt;
- zatrzymywanie niewidocznych animacji;
- ograniczenie przebudowy meshów;
- ustawienia render distance dla PC i Switcha;
- test dłuższej sesji zapisu/odczytu.

## 14. Czego nie robimy przed vertical slice

- multiplayera;
- nieskończonej wysokości świata;
- skomplikowanych płynów;
- pełnego oświetlenia globalnego;
- rozbudowanego craftingu;
- setek typów bloków;
- deformowanego skinningu postaci;
- rozbudowanego ECS-a;
- wykonywania FBX importera na Switchu;
- własnego zamiennika Blendera;
- optymalizacji bez pomiarów.

## 15. Zasady pracy Codexa w VS Code

1. Codex wykonuje tylko bieżący kamień milowy i nie przeskakuje do późniejszych systemów.
2. Przed zmianą czyta `PLAN.md`, `ARCHITECTURE.md` oraz instrukcje repozytorium.
3. Każda większa funkcja ma test lub jednoznaczną scenę diagnostyczną.
4. Po zmianie Codex buduje lokalnie PC i uruchamia właściwe testy.
5. Build NX jest wykonywany lokalnie, jeżeli toolchain jest dostępny, oraz zawsze w GitHub Actions.
6. Codex nie deklaruje sukcesu wersji Switch wyłącznie na podstawie udanej kompilacji. Odróżnia: `compiled`, `emulator-tested`, `hardware-tested`.
7. Kod platformowy trafia wyłącznie do `src/platform` i konfiguracji buildów.
8. Codex nie dodaje ciężkiej zależności bez uzasadnienia i sprawdzenia zgodności z devkitA64.
9. Zmiany formatów `.vxm`, `.vxa` i save'a zwiększają numer wersji oraz zachowują walidację.
10. Każdy commit ma mały zakres i czytelny opis.
11. Po pushu Codex sprawdza GitHub Actions i poprawia błąd CI, zanim etap zostanie zamknięty.
12. Nie wolno maskować błędów assetów fallbackiem do pustego modelu bez czytelnego komunikatu diagnostycznego.

## 16. Pierwsze zadanie implementacyjne

Codex ma rozpocząć od M0, a następnie M1. Pierwszy pull request powinien zawierać:

- szkielet repozytorium;
- aplikację PC z raylib;
- minimalny build raylib-nx;
- `CMakePresets.json`;
- zadania VS Code: build, run, test, build Switch;
- GitHub Actions dla desktop i Switch;
- ekran diagnostyczny pokazujący platformę, wersję i hash commita;
- instrukcję pobrania artefaktu `.nro` z telefonu.

Po przejściu M0 obowiązuje zaakceptowany priorytet voxel-first opisany w sekcji 13. Spike Lua i modelu hierarchicznego wracają po zbudowaniu fundamentu chunków, przed systemami gameplayu zależnymi od tych technologii.
