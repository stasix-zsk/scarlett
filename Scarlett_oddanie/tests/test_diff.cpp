// test_diff.cpp: test roznicowy - sprawdza, ze wynik VM (po pelnym kodowaniu
// i dekodowaniu) dla funkcji z petla while zgadza sie z wersja natywna C++
// dla wielu wartosci wejsciowych.
#include "scarlett/Parser.h"
#include "scarlett/Lower.h"
#include "scarlett/Backend.h"
#include "scarlett/Codec.h"
#include "scarlett/Runtime.h"
#include <cassert>
#include <iostream>
#include <vector>

static long long native(int n){
    long long s=0;
    // petla liczy sume kwadratow liczb od 0 do n (referencja dla VM)
    for(int i=0;i<=n;++i) s+=i*i;
    return s;
}

int main() {
    const char* src = R"(
[[vm_protect]] int sqsum(int n) {
    int s = 0;
    int i = 0;
    while (i <= n) {
        s = s + i * i;
        i = i + 1;
    }
    return s;
}
)";
    scarlett::Parser p(src);
    auto prog = p.parse();
    scarlett::IRModule ir = scarlett::lowerProgram(prog);
    scarlett::VMModule vm = scarlett::lowerToVM(ir);
    scarlett::EncodeResult enc = scarlett::encodeModule(vm, 999);
    std::string err; bool ok=false;
    scarlett::VMModule dec = scarlett::decodeModule(enc.bytes, err, &ok);
    assert(err.empty() && ok);
    scarlett::VM v;
    // petla porownuje wynik VM z wersja natywna dla n=0..50
    for (int n=0;n<=50;++n) {
        auto r = v.run(dec, "sqsum", {n});
        assert(r.ok);
        assert(r.value == native(n));
    }
    std::cout << "Testy roznicowe przeszly\n";
    return 0;
}
