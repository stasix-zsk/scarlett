// test_fuzz.cpp: test fuzzingowy - 1000 losowych wywolan VM funkcji sum(n)
// (suma liczb 0..n), porownanie wyniku ze wzorem n*(n+1)/2, majace wykryc
// rzadkie bledy interpretera.
#include "scarlett/Parser.h"
#include "scarlett/Lower.h"
#include "scarlett/Backend.h"
#include "scarlett/Codec.h"
#include "scarlett/Runtime.h"
#include <cassert>
#include <iostream>
#include <random>

int main() {
    const char* src = R"(
[[vm_protect]] int sum(int n) {
    int s = 0;
    int i = 0;
    while (i <= n) { s = s + i; i = i + 1; }
    return s;
}
)";
    scarlett::Parser p(src);
    auto prog = p.parse();
    scarlett::IRModule ir = scarlett::lowerProgram(prog);
    scarlett::VMModule vm = scarlett::lowerToVM(ir);
    scarlett::EncodeResult enc = scarlett::encodeModule(vm, 2024);
    std::string err; bool ok=false;
    scarlett::VMModule dec = scarlett::decodeModule(enc.bytes, err, &ok);
    assert(ok);
    scarlett::VM v;
    std::mt19937 rng(1);
    // petla wykonuje 1000 losowych wywolan VM i porownuje wynik ze wzorem
    for (int t=0;t<1000;++t) {
        int n = rng() % 40;
        auto r = v.run(dec, "sum", {n});
        long long exp = (long long)n*(n+1)/2;
        assert(r.ok && r.value == exp);
    }
    std::cout << "Testy fuzz przeszly\n";
    return 0;
}
