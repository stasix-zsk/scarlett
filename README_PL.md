# Scarlett VM

[![CI](https://github.com/stasix-zsk/scarlett/actions/workflows/ci.yml/badge.svg)](https://github.com/stasix-zsk/scarlett/actions/workflows/ci.yml)
[![Build](https://github.com/stasix-zsk/scarlett/actions/workflows/build.yml/badge.svg)](https://github.com/stasix-zsk/scarlett/actions/workflows/build.yml)
[![Latest release](https://img.shields.io/github/v/release/stasix-zsk/scarlett?include_prereleases&label=latest)](https://github.com/stasix-zsk/scarlett/releases/latest)
[![CodeQL](https://github.com/stasix-zsk/scarlett/actions/workflows/codeql.yml/badge.svg)](https://github.com/stasix-zsk/scarlett/actions/workflows/codeql.yml)

Obfuskator kodu C++ oparty na maszynie wirtualnej (rdzeń M4).


## Budowanie

Windows (Visual Studio 2022, z PowerShell w katalogu projektu):

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
ctest --test-dir build -C Release
```

Wynikowe pliki trafiają do `build\Release\` (m.in. `scarlett.exe`, `scarlett_runtime.lib`).
Uruchamiając cokolwiek z katalogu projektu w PowerShell, pamiętaj o prefiksie `.\` przed ścieżką
(np. `.\build\Release\scarlett.exe`, nie samo `build\Release\scarlett.exe`) — bez niego PowerShell
czasem próbuje potraktować pierwszy człon ścieżki jako nazwę modułu do zaimportowania.

## Użycie — generowanie chronionej wersji

```powershell
.\build\Release\scarlett.exe --input examples\demo.cpp --protect mul,factorial --outdir out --seed 42
.\build\Release\scarlett.exe --input examples\more.cpp --protect fib,sum,max2 --outdir out --seed 42
```

Każde wywołanie generuje w folderze `out` trzy pliki: `<nazwa>.protected.cpp`, `<nazwa>.scarlett`, `<nazwa>.meta`.
`.scarlett` i `.meta` to tylko podgląd/metadane zaszyfrowanego bloku — do uruchomienia programu
wystarczy sam `.protected.cpp`, bo zaszyfrowany bajtkod jest zaszyty bezpośrednio w jego kodzie
jako statyczna tablica bajtów.

## Uruchomienie chronionej wersji

`<nazwa>.protected.cpp` nie jest samowystarczalny — wywołuje `scarlett::scarlett_call_blob`
z biblioteki `scarlett_runtime`, więc musi być z nią zlinkowany (tak samo jak w `CMakeLists.txt`
dla `scarlett_runtime`/`scarlett`).

Projekt ma już gotowe, warunkowe cele CMake dla przykładów z `examples/`: `demo_protected`
(z `out/demo.protected.cpp`) i `more_protected` (z `out/more.protected.cpp`). Pojawiają się
w buildzie dopiero po wygenerowaniu odpowiedniego pliku `.protected.cpp` i ponownej konfiguracji:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
.\build\Release\demo_protected.exe
.\build\Release\more_protected.exe
```

Ważne: samo `cmake --build` nie wystarczy po wygenerowaniu nowego pliku `.protected.cpp` —
trzeba najpierw ponownie uruchomić `cmake -S . -B build ...`, żeby CMake zauważył nowy plik
i dodał dla niego cel budowania.

Dla porównania z wersją niechronioną (dowód, że obfuskacja nie zmienia wyniku):

```powershell
cl /std:c++17 /EHsc examples\demo.cpp /Fe:demo_original.exe
.\demo_original.exe
```

(wymaga uruchomienia z "Developer PowerShell for VS 2022", żeby `cl` było rozpoznawane).

Dodając ochronę dla kolejnego, własnego pliku źródłowego, trzeba dopisać analogiczny warunkowy
cel w `CMakeLists.txt` (`add_executable(<nazwa>_protected out/<nazwa>.protected.cpp)` +
`target_link_libraries(... PRIVATE scarlett_runtime)`) — obecne cele są dopisane na sztywno
pod `demo` i `more`, a nie generowane automatycznie dla dowolnej nazwy pliku.

## Testy

- `scarlett_tests` – VM, rekurencja, integralność
- `scarlett_diff_tests` – zgodność z wersją natywną
- `scarlett_fuzz_tests` – 1000 iteracji
- `scarlett_for_tests` – petle `for` (lowering i wykonanie)

## Zakres M4

Obsługiwany podzbiór C++: int/bool/void, arytmetyka, porównania, zmienne lokalne, if, while, for, wywołania funkcji, return, rekurencja.
Chronione funkcje działają end-to-end przez ABI (argumenty i wynik) i zwracają ten sam wynik co wersja natywna.

### Napisy (string)

Typ `string` (można pisać `string` lub `std::string`) jest obsługiwany jako pełnoprawny typ —
zmienne, parametry i wartości zwracane funkcji chronionych. Wspierane operacje:

- literały napisowe, np. `"abc"` (z sekwencjami ucieczki `\n \t \r \0 \\ \"`),
- konkatenacja operatorem `+`,
- porównanie `==` i `!=` (wynik `bool`/`int`),
- długość przez wbudowane `len(s)` (zwraca `int`; dostępne też jako `length(s)`/`size(s)`).

Przykład (`examples/strings.cpp`):

```cpp
[[vm_protect]] std::string join(std::string a, std::string b) {
    return a + "/" + b;              // konkatenacja + literał
}
[[vm_protect]] int same(std::string a, std::string b) {
    if (a == b) return 1;            // porównanie napisów
    return 0;
}
[[vm_protect]] std::string repeat(std::string s, int n) {
    std::string r = "";
    for (int i = 0; i < n; i = i + 1) r = r + s;
    return r;
}
```

Napisy w maszynie wirtualnej żyją na dedykowanej stercie — rejestr przechowuje uchwyt (indeks),
a literały trafiają do zaszyfrowanej puli w bloku `.scarlett` (format bajtkodu v2). Funkcje
o sygnaturze z napisem używają rozszerzonego ABI (`scarlett_call_blob_str` / wariant z `ints[]`+`strs[]`).

Celowo **nie** wspierane w tej wersji: indeksowanie/znaki (`s[i]`), porządkowanie napisów
(`<`, `>` itp.), mutacja w miejscu, metody w stylu `s.length()` (parser nie obsługuje `.`),
oraz mieszanie `int + string`. `len(s)` to funkcja wbudowana VM — plik źródłowy, który jej używa,
kompiluje się natywnie dopiero po obfuskacji (w chronionej funkcji jest zastępowana bajtkodem).

## Potoki budowania

- **Stabilny**: budowy Release na `main` i tagach, z publikowanymi wydaniami wersjonowanymi.
- **Niestabilny**: budowy Debug na pull requestach i branchach feature, przesylane jako artefakty CI bez wydania.

## Wydania i tagi

Przeplywy GitHub Actions utrzymuja niestabilny, przewijany tag/wydanie `latest` przy kazdym
udanym pushu do `main`. To wydanie zawsze zawiera swiezo zbudowane pliki binarne dla
Linux, Windows i macOS. Tagi wersjonowane `vX.Y.Z` to stabilny kanal wydan.


## Licencja

Projekt jest udostepniany na licencji PolyForm Perimeter License 1.0.0. Zobacz [LICENSE](LICENSE).
Copyright (c) 2026 Edmund Kavu i Stanisław Gilewski.
