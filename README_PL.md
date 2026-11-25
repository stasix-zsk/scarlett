# Scarlett VM

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

## Zakres M4

Obsługiwany podzbiór C++: int/bool/void, arytmetyka, porównania, zmienne lokalne, if, while, wywołania funkcji, return.
Chronione funkcje działają end-to-end przez ABI (argumenty i wynik) i zwracają ten sam wynik co wersja natywna.
