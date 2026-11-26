# Scarlett VM

[![CI](https://github.com/stasix-zsk/scarlett/actions/workflows/ci.yml/badge.svg)](https://github.com/stasix-zsk/scarlett/actions/workflows/ci.yml)[![Build](https://github.com/stasix-zsk/scarlett/actions/workflows/build.yml/badge.svg)](https://github.com/stasix-zsk/scarlett/actions/workflows/build.yml)[![Latest release](https://img.shields.io/github/v/release/stasix-zsk/scarlett?include_prereleases&label=latest)](https://github.com/stasix-zsk/scarlett/releases/latest)[![CodeQL](https://github.com/stasix-zsk/scarlett/actions/workflows/codeql.yml/badge.svg)](https://github.com/stasix-zsk/scarlett/actions/workflows/codeql.yml)

Un ofuscador de código C++ basado en una máquina virtual (núcleo M4).

## Edificio

Windows (Visual Studio 2022, desde PowerShell en el directorio del proyecto):

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
ctest --test-dir build -C Release
```

Las salidas de la compilación se colocan en`build\Release\`(incluido`scarlett.exe`y`scarlett_runtime.lib`).
Al ejecutar cualquier cosa desde el directorio del proyecto en PowerShell, recuerde el`.\`prefijo antes de la ruta
(por ej.`.\build\Release\scarlett.exe`, no`build\Release\scarlett.exe`) — sin él, PowerShell puede
De vez en cuando intente tratar el primer componente de ruta como un nombre de módulo para importar.

## Uso: generar una versión protegida

```powershell
.\build\Release\scarlett.exe --input examples\demo.cpp --protect mul,factorial --outdir out --seed 42
.\build\Release\scarlett.exe --input examples\more.cpp --protect fib,sum,max2 --outdir out --seed 42
```

Cada invocación genera tres archivos en el`out`carpeta:`<name>.protected.cpp`,`<name>.scarlett`,`<name>.meta`.
El`.scarlett`y`.meta`Los archivos son solo una vista previa/metadatos del bloque cifrado: para ejecutar el programa.
el`.protected.cpp`El archivo por sí solo es suficiente, porque el código de bytes cifrado está incrustado directamente en su código.
como una matriz de bytes estática.

## Ejecutando la versión protegida

`<name>.protected.cpp`no es autónomo: llama`scarlett::scarlett_call_blob`desde`scarlett_runtime`biblioteca, por lo que debe estar vinculado a ella (al igual que en`CMakeLists.txt`para`scarlett_runtime`/`scarlett`).

El proyecto ya proporciona objetivos CMake condicionales para los ejemplos en`examples/`:`demo_protected`(de`out/demo.protected.cpp`) y`more_protected`(de`out/more.protected.cpp`). Aparecen en el
construir sólo después del correspondiente`.protected.cpp`Se ha generado el archivo y el proyecto se ha reconfigurado:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
.\build\Release\demo_protected.exe
.\build\Release\more_protected.exe
```

Importante: una llanura`cmake --build`no es suficiente después de generar un nuevo`.protected.cpp`archivo—
primero debes volver a ejecutar`cmake -S . -B build ...`para que CMake note el nuevo archivo y agregue una compilación
objetivo para ello.

Para comparar con la versión desprotegida (prueba de que la ofuscación no cambia el resultado):

```powershell
cl /std:c++17 /EHsc examples\demo.cpp /Fe:demo_original.exe
.\demo_original.exe
```

(requiere ejecutar desde "Desarrollador PowerShell para VS 2022" para que`cl`es reconocido).

Para agregar protección a otro archivo fuente propio, agregue un destino condicional coincidente en`CMakeLists.txt`(`add_executable(<name>_protected out/<name>.protected.cpp)`+`target_link_libraries(... PRIVATE scarlett_runtime)`) — los objetivos existentes están codificados
para`demo`y`more`, no se genera automáticamente para nombres de archivos arbitrarios.

## Pruebas

-   `scarlett_tests`– VM, recursividad, integridad
-   `scarlett_diff_tests`– acuerdo con la versión nativa
-   `scarlett_fuzz_tests`– 1000 iteraciones
-   `scarlett_for_tests`–`for`descenso y ejecución del bucle

## alcance m4

Subconjunto de C++ soportado: int/bool/void, aritmética, comparaciones, variables locales, if, while, do- while, for, break, continue, llamadas a funciones, return, recursividad.
Las funciones protegidas funcionan de un extremo a otro a través de la ABI (argumentos y resultados) y devuelven el mismo resultado que la versión nativa.

### Instrumentos de cuerda

El`string`tipo (escrito como`string`o`std::string`) es un tipo de primera clase: funciona para
variables locales, parámetros y valores de retorno de funciones protegidas. Operaciones soportadas:

-   literales de cadena, p.`"abc"`(con escapes`\n \t \r \0 \\ \"`),
-   concatenación con`+`,
-   igualdad`==`y desigualdad`!=`(resultado`bool`/`int`),
-   longitud a través del incorporado`len(s)`(regresa`int`; también disponible como`length(s)`/`size(s)`).

Ejemplo (`examples/strings.cpp`):

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

Dentro de las cadenas de VM se encuentran en un montón dedicado: un registro contiene un identificador (índice) y literales.
entrar en un grupo cifrado por función en el`.scarlett`bloque (formato de código de bytes v2). Funciones
cuya firma contiene una cadena utiliza la ABI extendida (`scarlett_call_blob_str`/ el`ints[]`+`strs[]`variante).

Deliberadamente**no**admitido en esta versión: indexación/caracteres (`s[i]`), ordenamiento de cadenas
(`<`,`>`,…), mutación in situ, llamadas de estilo de método como`s.length()`(el analizador no
manejar`.`), y mezclando`int + string`.`len(s)`es una máquina virtual integrada: se compila un archivo fuente que la utiliza
de forma nativa solo después de la ofuscación (dentro de una función protegida se reemplaza por código de bytes).

## Construir tuberías

-   **Estable**: El lanzamiento se basa en`main`y etiquetas, con versiones versionadas publicadas.
-   **Inestable**: Depuración de compilaciones en solicitudes de extracción y ramas de funciones, cargadas como
    Artefactos de CI sin crear una versión.

## Lanzamientos y etiquetas

Los flujos de trabajo de GitHub Actions mantienen un funcionamiento inestable`latest`etiqueta/lanzamiento actualizado
en cada impulso exitoso para`main`. Esa versión siempre contiene contenido recién creado.
binarios para Linux, Windows y macOS. Versionado`vX.Y.Z`las etiquetas son el estable
canal de liberación.

Una etiqueta de lanzamiento versionada (`vX.Y.Z`) se crea en su lugar cuando se realiza una inserción o compilación.
ejecutar con parámetros de lanzamiento:

-   poner`[release]`en el mensaje de confirmación (o cambio`VERSION`), o
-   ejecutar el**Construir**flujo de trabajo manualmente con`release`habilitado y opcionalmente un`version`, o
-   ejecutar el**Etiqueta**flujo de trabajo con una etiqueta explícita como`v1.2.3`.

Las versiones versionadas también publican los archivos binarios compilados para todas las plataformas, no solo
los archivos fuente generados por GitHub.

## Licencia

Este proyecto tiene la licencia PolyForm Perimeter License 1.0.0. Ver[LICENCIA](LICENSE).
Copyright (c) 2026 Edmund Kavu y Stanisław Gilewski.
