# Scarlett VM

[![CI](https://github.com/stasix-zsk/scarlett/actions/workflows/ci.yml/badge.svg)](https://github.com/stasix-zsk/scarlett/actions/workflows/ci.yml)[![Build](https://github.com/stasix-zsk/scarlett/actions/workflows/build.yml/badge.svg)](https://github.com/stasix-zsk/scarlett/actions/workflows/build.yml)[![Latest release](https://img.shields.io/github/v/release/stasix-zsk/scarlett?include_prereleases&label=latest)](https://github.com/stasix-zsk/scarlett/releases/latest)[![CodeQL](https://github.com/stasix-zsk/scarlett/actions/workflows/codeql.yml/badge.svg)](https://github.com/stasix-zsk/scarlett/actions/workflows/codeql.yml)

Obfuskator kodu C++ oparty na maszynie wirtualnej (rdzeń M4).

## Budynek

Windows (Visual Studio 2022, z PowerShell w katalogu projektu):

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
ctest --test-dir build -C Release
```

Dane wyjściowe kompilacji są umieszczane w`build\Release\`(w tym`scarlett.exe`I`scarlett_runtime.lib`).
Uruchamiając cokolwiek z katalogu projektu w PowerShell, pamiętaj o`.\`przedrostek przed ścieżką
(np.`.\build\Release\scarlett.exe`, nie`build\Release\scarlett.exe`) — bez tego PowerShell może
czasami spróbuj potraktować pierwszy komponent ścieżki jako nazwę modułu do zaimportowania.

## Użycie — generowanie wersji chronionej

```powershell
.\build\Release\scarlett.exe --input examples\demo.cpp --protect mul,factorial --outdir out --seed 42
.\build\Release\scarlett.exe --input examples\more.cpp --protect fib,sum,max2 --outdir out --seed 42
```

Każde wywołanie generuje trzy pliki w formacie`out`falcówka:`<name>.protected.cpp`,`<name>.scarlett`,`<name>.meta`.
The`.scarlett`I`.meta`pliki stanowią jedynie podgląd/metadane zaszyfrowanego bloku — w celu uruchomienia programu
the`.protected.cpp`wystarczy sam plik, ponieważ zaszyfrowany kod bajtowy jest osadzony bezpośrednio w jego kodzie
jako statyczna tablica bajtów.

## Uruchamianie wersji chronionej

`<name>.protected.cpp`nie jest samowystarczalny — woła`scarlett::scarlett_call_blob`z`scarlett_runtime`biblioteka, więc musi być z nią połączona (tak jak w`CMakeLists.txt`Do`scarlett_runtime`/`scarlett`).

Projekt zapewnia już warunkowe cele CMake dla przykładów w`examples/`:`demo_protected`(z`out/demo.protected.cpp`) I`more_protected`(z`out/more.protected.cpp`). Pojawiają się w
budować dopiero po odpowiednim`.protected.cpp`plik został wygenerowany i projekt został ponownie skonfigurowany:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
.\build\Release\demo_protected.exe
.\build\Release\more_protected.exe
```

Ważne: zwykły`cmake --build`nie wystarczy po wygenerowaniu nowego`.protected.cpp`plik —
musisz najpierw powtórzyć`cmake -S . -B build ...`aby CMake zauważył nowy plik i dodał kompilację
cel dla niego.

Dla porównania z wersją niezabezpieczoną (dowód, że zaciemnianie nie zmienia wyniku):

```powershell
cl /std:c++17 /EHsc examples\demo.cpp /Fe:demo_original.exe
.\demo_original.exe
```

(wymaga uruchomienia z „Developer PowerShell dla VS 2022”, aby`cl`jest rozpoznawany).

Aby dodać ochronę innego własnego pliku źródłowego, dodaj pasujący cel warunkowy w`CMakeLists.txt`(`add_executable(<name>_protected out/<name>.protected.cpp)`+`target_link_libraries(... PRIVATE scarlett_runtime)`) — istniejące cele są zakodowane na stałe
dla`demo`I`more`, nie są generowane automatycznie dla dowolnych nazw plików.

## Testy

-   `scarlett_tests`– VM, rekurencja, integralność
-   `scarlett_diff_tests`– zgodność z wersją natywną
-   `scarlett_fuzz_tests`– 1000 iteracji
-   `scarlett_for_tests`–`for`opuszczanie i wykonanie pętli

## Luneta M4

Obsługiwany podzbiór C++: int/bool/void, arytmetyka, porównania, zmienne lokalne, if, while, for, wywołania funkcji, powrót, rekurencja.
Funkcje chronione działają kompleksowo poprzez ABI (argumenty i wynik) i zwracają ten sam wynik, co wersja natywna.

### Smyczki

The`string`typ (zapisany jako`string`Lub`std::string`) jest typem pierwszorzędnym — sprawdza się
zmienne lokalne, parametry i wartości zwracane chronionych funkcji. Obsługiwane operacje:

-   literały łańcuchowe, np.`"abc"`(z ucieczkami`\n \t \r \0 \\ \"`),
-   połączenie z`+`,
-   równość`==`i nierówność`!=`(wynik`bool`/`int`),
-   długość poprzez wbudowany`len(s)`(wraca`int`; dostępny również jako`length(s)`/`size(s)`).

Przykład (`examples/strings.cpp`):

```cpp
[[vm_protect]] std::string join(std::string a, std::string b) {
    return a + "/" + b;              // concatenation + literal
}
[[vm_protect]] int same(std::string a, std::string b) {
    if (a == b) return 1;            // string comparison
    return 0;
}
[[vm_protect]] std::string repeat(std::string s, int n) {
    std::string r = "";
    for (int i = 0; i < n; i = i + 1) r = r + s;
    return r;
}
```

Wewnątrz ciągów VM znajdują się na dedykowanej stercie — rejestr przechowuje uchwyt (indeks) i literały
przejdź do zaszyfrowanej puli funkcji w pliku`.scarlett`blok (format kodu bajtowego v2). Funkcje
którego podpis zawiera ciąg znaków, użyj rozszerzonego ABI (`scarlett_call_blob_str`/`ints[]`+`strs[]`wariant).

Celowo**nie**obsługiwane w tej wersji: indeksowanie/znaki (`s[i]`), kolejność ciągów
(`<`,`>`, …), mutacja lokalna, wywołania w stylu metody, takie jak`s.length()`(parser tego nie robi
uchwyt`.`) i miksuję`int + string`.`len(s)`jest wbudowaną maszyną wirtualną — plik źródłowy korzystający z niej kompiluje się
natywnie dopiero po zaciemnieniu (wewnątrz chronionej funkcji jest ona zastępowana przez kod bajtowy).

## Buduj rurociągi

-   **Stabilny**: Wydanie opiera się na`main`i tagi z opublikowanymi wersjami wersji.
-   **Nietrwały**: Debugowanie opiera się na żądaniach ściągnięcia i gałęziach funkcji przesłanych jako
    Artefakty CI bez tworzenia wersji.

## Wydania i tagi

Przepływy pracy GitHub Actions działają niestabilnie`latest`zaktualizowano tag/wydanie
przy każdym udanym naciśnięciu`main`. To wydanie zawsze zawiera świeżo zbudowane
pliki binarne dla systemów Linux, Windows i macOS. Wersjonowane`vX.Y.Z`tagi są stabilne
kanał wydania.

Wersjonowany tag wydania (`vX.Y.Z`) jest tworzony zamiast tego w przypadku wypychania lub kompilacji
uruchom z parametrami wydania:

-   umieścić`[release]`w komunikacie zatwierdzenia (lub zmień`VERSION`), Lub
-   uruchom**Zbudować**przepływ pracy ręcznie za pomocą`release`włączone i opcjonalnie a`version`, Lub
-   uruchom**Etykietka**przepływ pracy z wyraźnym tagiem, takim jak`v1.2.3`.

Wersjonowane wydania publikują również skompilowane pliki binarne dla wszystkich platform, a nie tylko
archiwa źródłowe wygenerowane przez GitHub.

## Licencja

Ten projekt jest objęty licencją PolyForm Perimeter License 1.0.0. Widzieć[LICENCJA](LICENSE).
Copyright (c) 2026 Edmund Kavu and Stanisław Gilewski.
