# 斯嘉丽虚拟机

[![CI](https://github.com/stasix-zsk/scarlett/actions/workflows/ci.yml/badge.svg)](https://github.com/stasix-zsk/scarlett/actions/workflows/ci.yml)[![Build](https://github.com/stasix-zsk/scarlett/actions/workflows/build.yml/badge.svg)](https://github.com/stasix-zsk/scarlett/actions/workflows/build.yml)[![Latest release](https://img.shields.io/github/v/release/stasix-zsk/scarlett?include_prereleases&label=latest)](https://github.com/stasix-zsk/scarlett/releases/latest)[![CodeQL](https://github.com/stasix-zsk/scarlett/actions/workflows/codeql.yml/badge.svg)](https://github.com/stasix-zsk/scarlett/actions/workflows/codeql.yml)

基于虚拟机（M4核心）的C++代码混淆器。

## 建筑

Windows（Visual Studio 2022，来自项目目录中的 PowerShell）：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
ctest --test-dir build -C Release
```

构建输出放置在`build\Release\`（包括`scarlett.exe`和`scarlett_runtime.lib`）。
在 PowerShell 中从项目目录运行任何内容时，请记住`.\`路径之前的前缀
（例如`.\build\Release\scarlett.exe`， 不是`build\Release\scarlett.exe`) — 没有它 PowerShell 可能
有时尝试将第一个路径组件视为要导入的模块名称。

## 用法 — 生成受保护的版本

```powershell
.\build\Release\scarlett.exe --input examples\demo.cpp --protect mul,factorial --outdir out --seed 42
.\build\Release\scarlett.exe --input examples\more.cpp --protect fib,sum,max2 --outdir out --seed 42
```

每次调用都会生成三个文件`out`文件夹：`<name>.protected.cpp`,`<name>.scarlett`,`<name>.meta`。
这`.scarlett`和`.meta`文件只是加密块的预览/元数据 - 运行程序
的`.protected.cpp`单独的文件就足够了，因为加密的字节码直接嵌入到其代码中
作为静态字节数组。

## 运行受保护的版本

`<name>.protected.cpp`不是独立的——它调用`scarlett::scarlett_call_blob`从`scarlett_runtime`库，所以它必须链接到它（就像`CMakeLists.txt`为了`scarlett_runtime`/`scarlett`).

该项目已经为以下示例提供了条件 CMake 目标`examples/`:`demo_protected`（从`out/demo.protected.cpp`） 和`more_protected`（从`out/more.protected.cpp`）。他们出现在
仅在相应的之后构建`.protected.cpp`文件已生成并重新配置项目：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
.\build\Release\demo_protected.exe
.\build\Release\more_protected.exe
```

重要：简单`cmake --build`生成新的后还不够`.protected.cpp`文件——
你必须首先重新运行`cmake -S . -B build ...`以便 CMake 注意到新文件并添加构建
为它设定的目标。

与未受保护的版本进行比较（证明混淆不会改变结果）：

```powershell
cl /std:c++17 /EHsc examples\demo.cpp /Fe:demo_original.exe
.\demo_original.exe
```

（需要从“Developer PowerShell for VS 2022”运行，以便`cl`被认可）。

要为您自己的另一个源文件添加保护，请在中添加匹配的条件目标`CMakeLists.txt`(`add_executable(<name>_protected out/<name>.protected.cpp)`+`target_link_libraries(... PRIVATE scarlett_runtime)`) — 现有目标是硬编码的
为了`demo`和`more`，不会为任意文件名自动生成。

## 测试

-   `scarlett_tests`– VM、递归、完整性
-   `scarlett_diff_tests`– 与原生版本一致
-   `scarlett_fuzz_tests`– 1000 次迭代
-   `scarlett_for_tests`–`for`循环降低和执行

## M4瞄准镜

支持的 C++ 子集：int/bool/void、算术、比较、局部变量、if、while、do-while、for、break、 continue、函数调用、return、递归。
受保护的函数通过 ABI（参数和结果）端到端工作，并返回与本机版本相同的结果。

### 弦乐

这`string`类型（写为`string`或者`std::string`) 是一流的类型——它适用于
受保护函数的局部变量、参数和返回值。支持的操作：

-   字符串文字，例如`"abc"`（有转义`\n \t \r \0 \\ \"`),
-   连接与`+`,
-   平等`==`和不平等`!=`（结果`bool`/`int`),
-   通过内置的长度`len(s)`（返回`int`;也可用作`length(s)`/`size(s)`).

例子 （`examples/strings.cpp`):

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

在虚拟机内部，字符串位于专用堆上——寄存器保存句柄（索引）和文字
进入加密的每个功能池`.scarlett`块（字节码格式 v2）。功能
其签名包含一个字符串，使用扩展 ABI (`scarlett_call_blob_str`/ 这`ints[]`+`strs[]`变体）。

故意地**不是**此版本支持：索引/字符（`s[i]`), 字符串排序
（`<`,`>`，...），就地突变，方法式调用，例如`s.length()`（解析器不
手柄`.`），并混合`int + string`.`len(s)`是一个内置的VM——使用它编译的源文件
原生地仅在混淆之后（在受保护的函数内它被字节码替换）。

## 建设管道

-   **稳定的**：发布建立在`main`和标签，以及已发布的版本。
-   **不稳定**：调试基于拉取请求和功能分支，上传为
    CI 工件无需创建版本。

## 版本和标签

GitHub Actions 工作流程不稳定`latest`标签/发布已更新
每一次成功的推动`main`。该版本始终包含新构建的
适用于 Linux、Windows 和 macOS 的二进制文件。版本控制`vX.Y.Z`标签是稳定的
发布通道。

版本化的发布标签（`vX.Y.Z`) 是在推送或构建时创建的
使用发布参数运行：

-   放`[release]`在提交消息中（或更改`VERSION`）， 或者
-   运行**建造**手动工作流程`release`启用并可选`version`， 或者
-   运行**标签**具有显式标签的工作流程，例如`v1.2.3`.

版本化版本还发布适用于所有平台的编译二进制文件，而不仅仅是
GitHub 生成的源档案。

## 执照

该项目根据 PolyForm Perimeter License 1.0.0 获得许可。看[执照](LICENSE)。
版权所有 (c) 2026 Edmund Kavu 和 Stanisław Gilewski。
