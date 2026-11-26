#include "scarlett/Parser.h"
#include "scarlett/Lower.h"
#include "scarlett/Backend.h"
#include "scarlett/Codec.h"
#include "scarlett/Runtime.h"
#include <cassert>
#include <iostream>

int main() {
    const char* src = R"(
[[vm_protect]] int dowhile(int n) {
    int i = 0;
    int s = 0;
    do {
        s = s + i;
        i = i + 1;
    } while (i < n);
    return s;
}

[[vm_protect]] int breaksum(int n) {
    int s = 0;
    for (int i = 0; i < n; i = i + 1) {
        if (i == 5) break;
        s = s + i;
    }
    return s;
}

[[vm_protect]] int contsum(int n) {
    int s = 0;
    for (int i = 0; i < n; i = i + 1) {
        if (i % 2 == 0) continue;
        s = s + i;
    }
    return s;
}
)";
    scarlett::Parser p(src);
    auto prog = p.parse();
    assert(p.error().empty());
    scarlett::IRModule ir = scarlett::lowerProgram(prog);
    scarlett::VMModule vm = scarlett::lowerToVM(ir);
    scarlett::EncodeResult enc = scarlett::encodeModule(vm, 123);
    std::string err; bool ok = false;
    scarlett::VMModule dec = scarlett::decodeModule(enc.bytes, err, &ok);
    assert(err.empty() && ok);
    scarlett::VM v;

    for (int n = 0; n <= 20; ++n) {
        int expected = 0;
        for (int i = 0; i < n; ++i) expected += i;
        if (n == 0) expected = 0; // body runs once with i=0
        auto r = v.run(dec, "dowhile", {n});
        assert(r.ok && r.value == expected);
    }
    for (int n = 0; n <= 20; ++n) {
        int expected = 0;
        for (int i = 0; i < n && i < 5; ++i) expected += i;
        auto r = v.run(dec, "breaksum", {n});
        assert(r.ok && r.value == expected);
    }
    for (int n = 0; n <= 20; ++n) {
        int expected = 0;
        for (int i = 0; i < n; ++i) if (i % 2 != 0) expected += i;
        auto r = v.run(dec, "contsum", {n});
        assert(r.ok && r.value == expected);
    }

    std::cout << "Testy control (do-while/break/continue) przeszly\n";
    return 0;
}
