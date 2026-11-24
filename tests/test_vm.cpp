// test_vm.cpp: sprawdza caly potok (parser -> IR -> VM -> kodowanie ->
// dekodowanie -> wykonanie) na rekurencyjnym factorial oraz weryfikuje,
// ze zepsucie bajtu zaszyfrowanych danych jest wykrywane przy dekodowaniu.
#include "scarlett/Parser.h"
#include "scarlett/Lower.h"
#include "scarlett/Backend.h"
#include "scarlett/Codec.h"
#include "scarlett/Runtime.h"
#include <cassert>
#include <iostream>
#include <vector>

int main() {
    const char* src = R"(
[[vm_protect]] int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}
)";
    scarlett::Parser p(src);
    auto prog = p.parse();
    scarlett::IRModule ir = scarlett::lowerProgram(prog);
    scarlett::VMModule vm = scarlett::lowerToVM(ir);
    scarlett::EncodeResult enc = scarlett::encodeModule(vm, 12345);
    std::string err; bool ok=false;
    scarlett::VMModule dec = scarlett::decodeModule(enc.bytes, err, &ok);
    assert(err.empty());
    assert(ok);
    scarlett::VM v;
    // petla porownuje wynik VM z wynikiem liczonym "na piechote" dla n=0..10
    for (int n = 0; n <= 10; ++n) {
        auto r = v.run(dec, "factorial", {n});
        long long exp = 1; for (int i=2;i<=n;++i) exp*=i; // liczy n! wprost, do porownania
        if (!r.ok || r.value != exp) {
            std::cerr << "FAIL factorial " << n << " got " << r.value << " err " << r.error << "\n";
            return 1;
        }
    }

    // test integralnosci: zepsucie bajtu musi zostac wykryte przy dekodowaniu
    std::vector<uint8_t> tampered = enc.bytes;
    tampered[tampered.size()-1] ^= 1;
    bool ok2=false; std::string e2;
    scarlett::decodeModule(tampered, e2, &ok2);
    assert(!ok2);
    std::cout << "Testy przeszly\n";
    return 0;
}
