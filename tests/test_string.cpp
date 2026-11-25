// test_string.cpp: testy roznicowe i fuzz dla obslugi typu string w Scarlett VM.
// Sprawdzaja caly potok (parser -> IR -> VM -> kodowanie -> dekodowanie ->
// wykonanie) dla literalow, konkatenacji (+), porownan (==/!=), dlugosci
// (len) oraz parametrow i wartosci zwracanych typu string, porownujac wynik
// z natywna wersja C++ (std::string). Dodatkowo weryfikuja sciezke ABI
// (scarlett_call_blob / scarlett_call_blob_str) uzywana przez wygenerowany kod.
#include "scarlett/Parser.h"
#include "scarlett/Lower.h"
#include "scarlett/Backend.h"
#include "scarlett/Codec.h"
#include "scarlett/Runtime.h"
#include <cassert>
#include <iostream>
#include <random>
#include <string>
#include <vector>

// --- wersje natywne (referencja dla VM) ---

static std::string native_band(int n) {
    std::string r;
    for (int i = 0; i < n; ++i) r += "ab";
    return r;
}
static long long native_lensum(int n) {
    std::string s;
    int i = 0;
    while (i < n) { s = s + "xy"; i = i + 1; }
    return (long long)s.size();
}
static std::string native_join(const std::string& a, const std::string& b) {
    return a + "/" + b;
}
static long long native_eqp(const std::string& a, const std::string& b) {
    return a == b ? 1 : 0;
}

// Zrodlo chronionych funkcji (podzbior wspierany przez Scarlett).
static const char* kSrc = R"(
[[vm_protect]] string band(int n) {
    string r = "";
    for (int i = 0; i < n; i = i + 1) {
        r = r + "ab";
    }
    return r;
}

[[vm_protect]] int lensum(int n) {
    string s = "";
    int i = 0;
    while (i < n) {
        s = s + "xy";
        i = i + 1;
    }
    return len(s);
}

[[vm_protect]] string join(string a, string b) {
    return a + "/" + b;
}

[[vm_protect]] int eqp(string a, string b) {
    if (a == b) return 1;
    return 0;
}

[[vm_protect]] int nep(string a, string b) {
    if (a != b) return 1;
    return 0;
}
)";

static scarlett::VMModule buildDecoded(const char* src, uint64_t seed, std::vector<uint8_t>& bytesOut) {
    scarlett::Parser p(src);
    auto prog = p.parse();
    assert(p.error().empty());
    scarlett::IRModule ir = scarlett::lowerProgram(prog);
    scarlett::VMModule vm = scarlett::lowerToVM(ir);
    scarlett::EncodeResult enc = scarlett::encodeModule(vm, seed);
    bytesOut = enc.bytes;
    std::string err; bool ok = false;
    scarlett::VMModule dec = scarlett::decodeModule(enc.bytes, err, &ok);
    assert(err.empty() && ok);
    return dec;
}

static std::string randStr(std::mt19937& rng) {
    static const char* alpha = "abcxyz/";
    int len = rng() % 6;
    std::string s;
    for (int i = 0; i < len; ++i) s += alpha[rng() % 7];
    return s;
}

int main() {
    std::vector<uint8_t> bytes;
    scarlett::VMModule dec = buildDecoded(kSrc, 7777, bytes);
    scarlett::VM v;

    // 1) roznicowo: band(n) zwraca string zbudowany konkatenacja w petli for
    for (int n = 0; n <= 40; ++n) {
        auto r = v.run(dec, "band", {n});
        assert(r.ok);
        if (r.str() != native_band(n)) {
            std::cerr << "FAIL band " << n << " got '" << r.str() << "' exp '" << native_band(n) << "'\n";
            return 1;
        }
    }

    // 2) roznicowo: lensum(n) = dlugosc napisu (len) budowanego w petli while
    for (int n = 0; n <= 40; ++n) {
        auto r = v.run(dec, "lensum", {n});
        assert(r.ok && r.value == native_lensum(n));
    }

    // 3) parametry i zwracanie string: join / eqp / nep (wariant runArgs)
    std::mt19937 rng(123);
    for (int t = 0; t < 1000; ++t) {
        std::string a = randStr(rng), b = randStr(rng);
        auto rj = v.runArgs(dec, "join", {a, b});
        assert(rj.ok && rj.str() == native_join(a, b));
        auto re = v.runArgs(dec, "eqp", {a, b});
        assert(re.ok && re.value == native_eqp(a, b));
        auto rn = v.runArgs(dec, "nep", {a, b});
        assert(rn.ok && rn.value == (native_eqp(a, b) ? 0 : 1));
    }

    // 4) sciezka ABI uzywana przez wygenerowany .protected.cpp:
    //    scarlett_call_blob_str (zwraca string) oraz scarlett_call_blob (ints+strs)
    for (int t = 0; t < 200; ++t) {
        std::string a = randStr(rng), b = randStr(rng);
        const char* strs[2] = { a.c_str(), b.c_str() };
        long long ints[2] = { 0, 0 };
        int ok = 0;
        std::string got = scarlett::scarlett_call_blob_str(bytes.data(), bytes.size(), "join", ints, strs, 2, &ok);
        assert(ok && got == native_join(a, b));
        int ok2 = 0;
        long long eq = scarlett::scarlett_call_blob(bytes.data(), bytes.size(), "eqp", ints, strs, 2, &ok2);
        assert(ok2 && eq == native_eqp(a, b));
    }

    // 5) integralnosc: zepsucie bajtu musi zostac wykryte przy dekodowaniu
    std::vector<uint8_t> tampered = bytes;
    tampered[tampered.size() - 1] ^= 1;
    bool ok3 = false; std::string e3;
    scarlett::decodeModule(tampered, e3, &ok3);
    assert(!ok3);

    std::cout << "Testy string (roznicowe + fuzz + ABI) przeszly\n";
    return 0;
}
