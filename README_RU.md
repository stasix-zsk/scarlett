# Скарлетт ВМ

[![CI](https://github.com/stasix-zsk/scarlett/actions/workflows/ci.yml/badge.svg)](https://github.com/stasix-zsk/scarlett/actions/workflows/ci.yml)[![Build](https://github.com/stasix-zsk/scarlett/actions/workflows/build.yml/badge.svg)](https://github.com/stasix-zsk/scarlett/actions/workflows/build.yml)[![Latest release](https://img.shields.io/github/v/release/stasix-zsk/scarlett?include_prereleases&label=latest)](https://github.com/stasix-zsk/scarlett/releases/latest)[![CodeQL](https://github.com/stasix-zsk/scarlett/actions/workflows/codeql.yml/badge.svg)](https://github.com/stasix-zsk/scarlett/actions/workflows/codeql.yml)

Обфускатор кода C++ на базе виртуальной машины (ядро M4).

## Здание

Windows (Visual Studio 2022, из PowerShell в каталоге проекта):

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
ctest --test-dir build -C Release
```

Результаты сборки помещаются в`build\Release\`(включая`scarlett.exe`и`scarlett_runtime.lib`).
При запуске чего-либо из каталога проекта в PowerShell помните`.\`префикс перед путем
(например`.\build\Release\scarlett.exe`, нет`build\Release\scarlett.exe`) — без него PowerShell может
иногда пытайтесь рассматривать первый компонент пути как имя импортируемого модуля.

## Использование — создание защищенной версии

```powershell
.\build\Release\scarlett.exe --input examples\demo.cpp --protect mul,factorial --outdir out --seed 42
.\build\Release\scarlett.exe --input examples\more.cpp --protect fib,sum,max2 --outdir out --seed 42
```

Каждый вызов генерирует три файла в папке`out`папка:`<name>.protected.cpp`,`<name>.scarlett`,`<name>.meta`.
`.scarlett`и`.meta`файлы представляют собой лишь предварительный просмотр/метаданные зашифрованного блока — для запуска программы
тот`.protected.cpp`одного файла достаточно, поскольку зашифрованный байт-код встроен непосредственно в его код
как статический массив байтов.

## Запуск защищенной версии

`<name>.protected.cpp`не является самодостаточным — он вызывает`scarlett::scarlett_call_blob`из`scarlett_runtime`библиотеку, поэтому она должна быть скомпонована с ней (как в`CMakeLists.txt`для`scarlett_runtime`/`scarlett`).

Проект уже предоставляет условные цели CMake для примеров в`examples/`:`demo_protected`(от`out/demo.protected.cpp`) и`more_protected`(от`out/more.protected.cpp`). Они появляются в
строить только после соответствующего`.protected.cpp`файл был сгенерирован и проект переконфигурирован:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
.\build\Release\demo_protected.exe
.\build\Release\more_protected.exe
```

Важно: однотонный`cmake --build`недостаточно после создания нового`.protected.cpp`файл —
сначала тебе нужно перезапустить`cmake -S . -B build ...`чтобы CMake заметил новый файл и добавил сборку
цель для этого.

Для сравнения с незащищенной версией (доказательство того, что обфускация не меняет результат):

```powershell
cl /std:c++17 /EHsc examples\demo.cpp /Fe:demo_original.exe
.\demo_original.exe
```

(требуется запуск из «Разработчика PowerShell для VS 2022», чтобы`cl`признается).

Чтобы добавить защиту для другого исходного файла, добавьте соответствующую условную цель в`CMakeLists.txt`(`add_executable(<name>_protected out/<name>.protected.cpp)`+`target_link_libraries(... PRIVATE scarlett_runtime)`) — существующие цели жестко запрограммированы
для`demo`и`more`, не генерируется автоматически для произвольных имен файлов.

## Тесты

-   `scarlett_tests`– VM, рекурсия, целостность
-   `scarlett_diff_tests`— согласие с нативной версией
-   `scarlett_fuzz_tests`– 1000 итераций
-   `scarlett_for_tests`–`for`опускание и выполнение петли

## прицел М4

Поддерживаемое подмножество C++: int/bool/void, арифметика, сравнения, локальные переменные, if, while, for, вызовы функций, возврат, рекурсия.
Защищенные функции работают сквозно через ABI (аргументы и результат) и возвращают тот же результат, что и собственная версия.

### Струны

`string`тип (записывается как`string`или`std::string`) — первоклассный тип — он работает для
локальные переменные, параметры и возвращаемые значения защищенных функций. Поддерживаемые операции:

-   строковые литералы, например`"abc"`(с побегами`\n \t \r \0 \\ \"`),
-   конкатенация с`+`,
-   равенство`==`и неравенство`!=`(результат`bool`/`int`),
-   длина через встроенный`len(s)`(возвращается`int`; также доступен как`length(s)`/`size(s)`).

Пример (`examples/strings.cpp`):

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

Внутри виртуальной машины строки хранятся в выделенной куче — регистр содержит дескриптор (индекс) и литералы.
перейти в зашифрованный пул для каждой функции в`.scarlett`блок (формат байт-кода v2). Функции
чья подпись содержит строку, используйте расширенный ABI (`scarlett_call_blob_str`/`ints[]`+`strs[]`вариант).

Умышленно**нет**в этой версии поддерживается: индексация/символы (`s[i]`), порядок строк
(`<`,`>`, …), мутация на месте, вызовы в стиле метода, такие как`s.length()`(парсер не
ручка`.`) и смешивание`int + string`.`len(s)`является встроенной функцией виртуальной машины — с ее помощью компилируется исходный файл
изначально только после обфускации (внутри защищенной функции он заменяется байт-кодом).

## Стройте трубопроводы

-   **Стабильный**: Релиз основан на`main`и теги с опубликованными версиями релизов.
-   **Нестабильный**: Отладка строится на основе запросов на включение и ветвей функций, загруженных как
    Артефакты CI без создания релиза.

## Релизы и теги

Рабочие процессы GitHub Actions продолжают работать нестабильно.`latest`тег/выпуск обновлен
при каждом успешном нажатии на`main`. Этот выпуск всегда содержит только что созданные
двоичные файлы для Linux, Windows и macOS. Версия`vX.Y.Z`теги стабильны
канал выпуска.

Версионный тег выпуска (`vX.Y.Z`) вместо этого создается при отправке или сборке
запустить с параметрами выпуска:

-   помещать`[release]`в сообщении фиксации (или измените`VERSION`), или
-   запустить**Строить**рабочий процесс вручную с`release`включен и, при необходимости,`version`, или
-   запустить**Ярлык**рабочий процесс с явным тегом, например`v1.2.3`.

Версионные выпуски также публикуют скомпилированные двоичные файлы для всех платформ, а не только
исходные архивы, созданные GitHub.

## Лицензия

Этот проект распространяется по лицензии PolyForm Perimeter License 1.0.0. Видеть[ЛИЦЕНЗИЯ](LICENSE).
Copyright (c) 2026 Эдмунд Каву и Станислав Гилевский.
