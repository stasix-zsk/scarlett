// test_for.cpp: test roznicowy i fuzz dla petli for - sprawdza, ze wynik VM
// (po pelnym kodowaniu i dekodowaniu) zgadza sie z wersja natywna C++ dla
// roznych wariantow for: klasyczny, bez klamer, zagniezdzony, z pusta klauzula
// init/krok oraz z pustym warunkiem (for(;;)) i returnem w srodku.
#include "scarlett/Parser.h"
#include "scarlett/Lower.h"
#include "scarlett/Backend.h"
#include "scarlett/Codec.h"
#include "scarlett/Runtime.h"
#include <cassert>
#include <iostream>
#include <random>

// --- referencje natywne (wzorzec dla VM) ---
static long long nat_sqsum(int n){
    long long s=0;
    for(int i=0;i<=n;++i) s+=static_cast<long long>(i)*i;
    return s;
}
static long long nat_tri(int n){
    long long s=0;
    for(int i=1;i<=n;++i) s+=i;
    return s;
}
static long long nat_grid(int n){
    long long s=0;
    for(int i=0;i<n;++i) for(int j=0;j<n;++j) s+=static_cast<long long>(i)*j;
    return s;
}
static long long nat_countdown(int n){
    long long c=0;
    for(int i=n;i>0;) { ++c; --i; }
    return c;
}
static long long nat_firstge(int n){
    for(int i=0;;++i) if(static_cast<long long>(i)*i>=n) return i;
}

int main() {
    const char* src = R"(
// klasyczny for z klamrami
[[vm_protect]] int sqsum(int n) {
    int s = 0;
    for (int i = 0; i <= n; i = i + 1) {
        s = s + i * i;
    }
    return s;
}

// for bez klamer (jednoinstrukcyjne cialo)
[[vm_protect]] int tri(int n) {
    int s = 0;
    for (int i = 1; i <= n; i = i + 1) s = s + i;
    return s;
}

// zagniezdzona petla for
[[vm_protect]] int grid(int n) {
    int s = 0;
    for (int i = 0; i < n; i = i + 1) {
        for (int j = 0; j < n; j = j + 1) {
            s = s + i * j;
        }
    }
    return s;
}

// for z pusta klauzula init i pustym krokiem
[[vm_protect]] int countdown(int n) {
    int c = 0;
    int i = n;
    for (; i > 0; ) { c = c + 1; i = i - 1; }
    return c;
}

// for z pustym warunkiem (nieskonczony) przerywany przez return
[[vm_protect]] int firstge(int n) {
    for (int i = 0; ; i = i + 1) {
        if (i * i >= n) return i;
    }
}
)";
    scarlett::Parser p(src);
    auto prog = p.parse();
    assert(p.error().empty());
    scarlett::IRModule ir = scarlett::lowerProgram(prog);
    scarlett::VMModule vm = scarlett::lowerToVM(ir);
    scarlett::EncodeResult enc = scarlett::encodeModule(vm, 777);
    std::string err; bool ok=false;
    scarlett::VMModule dec = scarlett::decodeModule(enc.bytes, err, &ok);
    assert(err.empty() && ok);
    scarlett::VM v;

    // petla porownuje wyniki VM z wersjami natywnymi dla n=0..40
    for (int n=0;n<=40;++n) {
        auto r1 = v.run(dec, "sqsum", {n});     assert(r1.ok && r1.value == nat_sqsum(n));
        auto r2 = v.run(dec, "tri", {n});       assert(r2.ok && r2.value == nat_tri(n));
        auto r3 = v.run(dec, "grid", {n});      assert(r3.ok && r3.value == nat_grid(n));
        auto r4 = v.run(dec, "countdown", {n}); assert(r4.ok && r4.value == nat_countdown(n));
        auto r5 = v.run(dec, "firstge", {n});   assert(r5.ok && r5.value == nat_firstge(n));
    }

    // sekcja fuzz: 1000 losowych wywolan kazdej funkcji for i porownanie z natywna
    std::mt19937 rng(42);
    for (int t=0;t<1000;++t) {
        int n = rng() % 60;
        auto r1 = v.run(dec, "sqsum", {n});     assert(r1.ok && r1.value == nat_sqsum(n));
        auto r2 = v.run(dec, "tri", {n});       assert(r2.ok && r2.value == nat_tri(n));
        auto r4 = v.run(dec, "countdown", {n}); assert(r4.ok && r4.value == nat_countdown(n));
        auto r5 = v.run(dec, "firstge", {n});   assert(r5.ok && r5.value == nat_firstge(n));
        int m = rng() % 25;
        auto r3 = v.run(dec, "grid", {m});      assert(r3.ok && r3.value == nat_grid(m));
    }

    std::cout << "Testy for (roznicowe + fuzz) przeszly\n";
    return 0;
}
