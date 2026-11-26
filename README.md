# Scarlett VM

[![CI](https://github.com/stasix-zsk/scarlett/actions/workflows/ci.yml/badge.svg)](https://github.com/stasix-zsk/scarlett/actions/workflows/ci.yml)
[![Build](https://github.com/stasix-zsk/scarlett/actions/workflows/build.yml/badge.svg)](https://github.com/stasix-zsk/scarlett/actions/workflows/build.yml)
[![Latest release](https://img.shields.io/github/v/release/stasix-zsk/scarlett?include_prereleases&label=latest)](https://github.com/stasix-zsk/scarlett/releases/latest)
[![CodeQL](https://github.com/stasix-zsk/scarlett/actions/workflows/codeql.yml/badge.svg)](https://github.com/stasix-zsk/scarlett/actions/workflows/codeql.yml)

A C++ code obfuscator based on a virtual machine (M4 core).

## Building

Windows (Visual Studio 2022, from PowerShell in the project directory):

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
ctest --test-dir build -C Release
```

Build outputs are placed in `build\Release\` (including `scarlett.exe` and `scarlett_runtime.lib`).
When running anything from the project directory in PowerShell, remember the `.\` prefix before the path
(e.g. `.\build\Release\scarlett.exe`, not `build\Release\scarlett.exe`) — without it PowerShell may
occasionally try to treat the first path component as a module name to import.

## Usage — generating a protected version

```powershell
.\build\Release\scarlett.exe --input examples\demo.cpp --protect mul,factorial --outdir out --seed 42
.\build\Release\scarlett.exe --input examples\more.cpp --protect fib,sum,max2 --outdir out --seed 42
```

Each invocation generates three files in the `out` folder: `<name>.protected.cpp`, `<name>.scarlett`, `<name>.meta`.
The `.scarlett` and `.meta` files are only a preview/metadata of the encrypted block — to run the program
the `.protected.cpp` file alone is enough, because the encrypted bytecode is embedded directly in its code
as a static byte array.

## Running the protected version

`<name>.protected.cpp` is not self-contained — it calls `scarlett::scarlett_call_blob`
from the `scarlett_runtime` library, so it must be linked against it (just like in `CMakeLists.txt`
for `scarlett_runtime`/`scarlett`).

The project already provides conditional CMake targets for the examples in `examples/`: `demo_protected`
(from `out/demo.protected.cpp`) and `more_protected` (from `out/more.protected.cpp`). They appear in the
build only after the corresponding `.protected.cpp` file has been generated and the project reconfigured:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
.\build\Release\demo_protected.exe
.\build\Release\more_protected.exe
```

Important: a plain `cmake --build` is not enough after generating a new `.protected.cpp` file —
you must first rerun `cmake -S . -B build ...` so that CMake notices the new file and adds a build
target for it.

For comparison with the unprotected version (proof that obfuscation does not change the result):

```powershell
cl /std:c++17 /EHsc examples\demo.cpp /Fe:demo_original.exe
.\demo_original.exe
```

(requires running from "Developer PowerShell for VS 2022" so that `cl` is recognized).

To add protection for another source file of your own, add a matching conditional target in
`CMakeLists.txt` (`add_executable(<name>_protected out/<name>.protected.cpp)` +
`target_link_libraries(... PRIVATE scarlett_runtime)`) — the existing targets are hard-coded
for `demo` and `more`, not generated automatically for arbitrary file names.

## Tests

- `scarlett_tests` – VM, recursion, integrity
- `scarlett_diff_tests` – agreement with the native version
- `scarlett_fuzz_tests` – 1000 iterations
- `scarlett_for_tests` – `for` loop lowering and execution

## M4 scope

Supported C++ subset: int/bool/void, arithmetic, comparisons, local variables, if, while, do-while, for, break, continue, function calls, return, recursion.
Protected functions work end-to-end through the ABI (arguments and result) and return the same result as the native version.

### Strings

The `string` type (written as `string` or `std::string`) is a first-class type — it works for
local variables, parameters and return values of protected functions. Supported operations:

- string literals, e.g. `"abc"` (with escapes `\n \t \r \0 \\ \"`),
- concatenation with `+`,
- equality `==` and inequality `!=` (result `bool`/`int`),
- length via the built-in `len(s)` (returns `int`; also available as `length(s)`/`size(s)`).

Example (`examples/strings.cpp`):

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

Inside the VM strings live on a dedicated heap — a register holds a handle (index) and literals
go into an encrypted per-function pool in the `.scarlett` block (bytecode format v2). Functions
whose signature contains a string use the extended ABI (`scarlett_call_blob_str` / the `ints[]`+`strs[]`
variant).

Deliberately **not** supported in this version: indexing/characters (`s[i]`), string ordering
(`<`, `>`, …), in-place mutation, method-style calls such as `s.length()` (the parser does not
handle `.`), and mixing `int + string`. `len(s)` is a VM built-in — a source file using it compiles
natively only after obfuscation (inside a protected function it is replaced by bytecode).

## Build pipelines

- **Stable**: Release builds on `main` and tags, with published versioned releases.
- **Unstable**: Debug builds on pull requests and feature branches, uploaded as
  CI artifacts without creating a release.

## Releases and tags

The GitHub Actions workflows keep an unstable rolling `latest` tag/release updated
on every successful push to `main`. That release always contains freshly built
binaries for Linux, Windows and macOS. Versioned `vX.Y.Z` tags are the stable
release channel.

A versioned release tag (`vX.Y.Z`) is created instead when a push or build is
run with release parameters:

- put `[release]` in the commit message (or change `VERSION`), or
- run the **Build** workflow manually with `release` enabled and optionally a
  `version`, or
- run the **Tag** workflow with an explicit tag such as `v1.2.3`.

Versioned releases also publish the compiled binaries for all platforms, not just
the source archives generated by GitHub.

## License

This project is licensed under the PolyForm Perimeter License 1.0.0. See [LICENSE](LICENSE).
Copyright (c) 2026 Edmund Kavu and Stanisław Gilewski.
