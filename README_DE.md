# Scarlett VM

[![CI](https://github.com/stasix-zsk/scarlett/actions/workflows/ci.yml/badge.svg)](https://github.com/stasix-zsk/scarlett/actions/workflows/ci.yml)[![Build](https://github.com/stasix-zsk/scarlett/actions/workflows/build.yml/badge.svg)](https://github.com/stasix-zsk/scarlett/actions/workflows/build.yml)[![Latest release](https://img.shields.io/github/v/release/stasix-zsk/scarlett?include_prereleases&label=latest)](https://github.com/stasix-zsk/scarlett/releases/latest)[![CodeQL](https://github.com/stasix-zsk/scarlett/actions/workflows/codeql.yml/badge.svg)](https://github.com/stasix-zsk/scarlett/actions/workflows/codeql.yml)

Ein C++-Code-Obfuscator basierend auf einer virtuellen Maschine (M4-Kern).

## Gebäude

Windows (Visual Studio 2022, aus PowerShell im Projektverzeichnis):

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
ctest --test-dir build -C Release
```

Build-Ausgaben werden eingefügt`build\Release\`(einschließlich`scarlett.exe`Und`scarlett_runtime.lib`).
Wenn Sie etwas aus dem Projektverzeichnis in PowerShell ausführen, denken Sie daran`.\`Präfix vor dem Pfad
(z.B.`.\build\Release\scarlett.exe`, nicht`build\Release\scarlett.exe`) – ohne PowerShell möglicherweise
Versuchen Sie gelegentlich, die erste Pfadkomponente als zu importierenden Modulnamen zu behandeln.

## Verwendung – Generieren einer geschützten Version

```powershell
.\build\Release\scarlett.exe --input examples\demo.cpp --protect mul,factorial --outdir out --seed 42
.\build\Release\scarlett.exe --input examples\more.cpp --protect fib,sum,max2 --outdir out --seed 42
```

Jeder Aufruf generiert drei Dateien im`out`Ordner:`<name>.protected.cpp`,`<name>.scarlett`,`<name>.meta`.
Der`.scarlett`Und`.meta`Dateien sind nur eine Vorschau/Metadaten des verschlüsselten Blocks – um das Programm auszuführen
die`.protected.cpp`Die Datei allein reicht aus, da der verschlüsselte Bytecode direkt in ihren Code eingebettet ist
als statisches Byte-Array.

## Ausführen der geschützten Version

`<name>.protected.cpp`ist nicht in sich geschlossen – es ruft`scarlett::scarlett_call_blob`aus dem`scarlett_runtime`Bibliothek, also muss es damit verlinkt werden (genau wie in`CMakeLists.txt`für`scarlett_runtime`/`scarlett`).

Das Projekt stellt bereits bedingte CMake-Ziele für die Beispiele in bereit`examples/`:`demo_protected`(aus`out/demo.protected.cpp`) Und`more_protected`(aus`out/more.protected.cpp`). Sie erscheinen in der
erst nach dem entsprechenden bauen`.protected.cpp`Datei wurde generiert und das Projekt neu konfiguriert:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
.\build\Release\demo_protected.exe
.\build\Release\more_protected.exe
```

Wichtig: eine Ebene`cmake --build`reicht nach der Generierung eines neuen nicht aus`.protected.cpp`Datei –
Sie müssen es zuerst erneut ausführen`cmake -S . -B build ...`damit CMake die neue Datei erkennt und einen Build hinzufügt
Ziel dafür.

Zum Vergleich mit der ungeschützten Version (Beweis, dass die Verschleierung das Ergebnis nicht verändert):

```powershell
cl /std:c++17 /EHsc examples\demo.cpp /Fe:demo_original.exe
.\demo_original.exe
```

(Erfordert die Ausführung von „Developer PowerShell für VS 2022“, damit`cl`wird erkannt).

Um Schutz für eine andere eigene Quelldatei hinzuzufügen, fügen Sie ein passendes bedingtes Ziel hinzu`CMakeLists.txt`(`add_executable(<name>_protected out/<name>.protected.cpp)`+`target_link_libraries(... PRIVATE scarlett_runtime)`) – die vorhandenen Ziele sind fest codiert
für`demo`Und`more`, wird für beliebige Dateinamen nicht automatisch generiert.

## Tests

-   `scarlett_tests`– VM, Rekursion, Integrität
-   `scarlett_diff_tests`– Zustimmung zur nativen Version
-   `scarlett_fuzz_tests`– 1000 Iterationen
-   `scarlett_for_tests`–`for`Schleifenabsenkung und -ausführung

## M4-Zielfernrohr

Unterstützte C++-Teilmenge: int/bool/void, Arithmetik, Vergleiche, lokale Variablen, if, while, for, Funktionsaufrufe, Rückgabe, Rekursion.
Geschützte Funktionen arbeiten durchgängig über die ABI (Argumente und Ergebnis) und geben das gleiche Ergebnis wie die native Version zurück.

### Saiten

Der`string`Typ (geschrieben als`string`oder`std::string`) ist ein erstklassiger Typ – er funktioniert für
lokale Variablen, Parameter und Rückgabewerte geschützter Funktionen. Unterstützte Operationen:

-   String-Literale, z.B.`"abc"`(mit Fluchten`\n \t \r \0 \\ \"`),
-   Verkettung mit`+`,
-   Gleichwertigkeit`==`und Ungleichheit`!=`(Ergebnis`bool`/`int`),
-   Länge über die eingebaute`len(s)`(kommt zurück`int`; auch erhältlich als`length(s)`/`size(s)`).

Beispiel (`examples/strings.cpp`):

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

Innerhalb der VM befinden sich Strings auf einem dedizierten Heap – ein Register enthält ein Handle (Index) und Literale
Gehen Sie in einen verschlüsselten Pool pro Funktion im`.scarlett`Block (Bytecode-Format v2). Funktionen
deren Signatur eine Zeichenfolge enthält, verwenden Sie die erweiterte ABI (`scarlett_call_blob_str`/ Die`ints[]`+`strs[]`Variante).

Absichtlich**nicht**In dieser Version unterstützt: indexing/characters (`s[i]`), String-Reihenfolge
(`<`,`>`, …), In-Place-Mutation, Aufrufe im Methodenstil wie z`s.length()`(Der Parser nicht
Griff`.`) und Mischen`int + string`.`len(s)`ist eine integrierte VM – eine Quelldatei, die sie kompiliert
nativ nur nach Verschleierung (innerhalb einer geschützten Funktion wird es durch Bytecode ersetzt).

## Bauen Sie Pipelines

-   **Stabil**: Release baut auf auf`main`und Tags, mit veröffentlichten versionierten Versionen.
-   **Instabil**: Debug-Builds auf Pull-Requests und Feature-Branches, hochgeladen als
    CI-Artefakte, ohne ein Release zu erstellen.

## Veröffentlichungen und Tags

Die GitHub Actions-Workflows laufen weiterhin instabil`latest`Tag/Release aktualisiert
bei jedem erfolgreichen Vorstoß`main`. Diese Version enthält immer frisch erstellte Dateien
Binärdateien für Linux, Windows und macOS. Versioniert`vX.Y.Z`Tags sind die stabilen
Release-Kanal.

Ein versioniertes Release-Tag (`vX.Y.Z`) wird stattdessen erstellt, wenn ein Push oder Build ausgeführt wird
Mit Release-Parametern ausführen:

-   setzen`[release]`in der Commit-Nachricht (oder change`VERSION`), oder
-   Führen Sie die aus**Bauen**Workflow manuell mit`release`aktiviert und optional a`version`, oder
-   Führen Sie die aus**Tag**Workflow mit einem expliziten Tag wie`v1.2.3`.

Versionierte Versionen veröffentlichen auch die kompilierten Binärdateien für alle Plattformen, nicht nur
die von GitHub generierten Quellarchive.

## Lizenz

Dieses Projekt ist unter der PolyForm Perimeter License 1.0.0 lizenziert. Sehen[LIZENZ](LICENSE).
Copyright (c) 2026 Edmund Kavu und Stanisław Gilewski.
