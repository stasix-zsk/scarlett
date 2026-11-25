#pragma once
// VM.h: VMFunction (pojedyncza funkcja gotowa do wykonania) oraz VMModule
// (caly modul VM - zbior funkcji + seed/rawBlock).
// Jedyna petla to liniowe wyszukiwanie funkcji po nazwie w indexOf.
#include "Types.h"
#include "Bytecode.h"
#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace scarlett {

struct VMFunction {
    std::string name;
    ValueType retType = ValueType::Int;
    int paramCount = 0;
    int regCount = 0;
    bool isProtected = false;
    bool isNative = false;
    std::vector<VMInst> code;
    std::vector<std::string> strpool;  // pula literalow napisowych (LOADS indeksuje po niej)

    std::function<long long(void*, const long long*, int)> native;
};

struct VMModule {
    std::vector<VMFunction> functions;
    std::vector<uint8_t> rawBlock;
    uint64_t seed = 0;

    int indexOf(const std::string& name) const {
        // liniowe przeszukanie listy funkcji po nazwie
        for (size_t i=0;i<functions.size();++i) if (functions[i].name==name) return (int)i;
        return -1;
    }
};

}
