#pragma once
// IR.h: reprezentacja posrednia (IR) - IROp, IRInst, IRFunction, IRModule.
// Podobne do Bytecode.h, ale skoki wskazuja etykiety tekstowe, nie adresy.
// Jedyne petle to wyszukiwanie funkcji po nazwie w IRModule::find.
#include "Types.h"
#include <cstdint>
#include <string>
#include <vector>
#include <utility>

namespace scarlett {

enum class IROp {
    NOP,
    LOADI,
    MOV,
    ADD, SUB, MUL, DIV, MOD,
    NEG,
    CMPEQ, CMPNE, CMPLT, CMPLE, CMPGT, CMPGE,
    AND, OR, XOR, NOT,
    JMP, JMPIF,
    CALL, RET, RETV,
    LABEL
};

struct IRInst {
    IROp op = IROp::NOP;
    int dst = -1;
    int a = -1;
    int b = -1;
    long long imm = 0;
    std::string label;      // nazwa etykiety (dla LABEL/JMP/JMPIF)
    std::string func;       // nazwa funkcji (dla CALL)
    std::vector<int> args;  // rejestry argumentow wywolania (dla CALL)
    int line = 0;
};

struct IRFunction {
    std::string name;
    ValueType retType = ValueType::Int;
    std::vector<std::pair<ValueType, std::string>> params;
    std::vector<IRInst> code;
    int regCount = 0;          // liczba rejestrow uzytych przez funkcje
    bool isProtected = false;  // czy funkcja ma byc chroniona (vm_protect)
    std::string vmName;        // losowa nazwa funkcji uzywana w VM
};

struct IRModule {
    std::vector<IRFunction> functions;

    IRFunction* find(const std::string& name) {
        // szuka funkcji po nazwie liniowo po calej liscie
        for (auto& f : functions) if (f.name == name) return &f;
        return nullptr;
    }
    const IRFunction* find(const std::string& name) const {
        // jak wyzej, wersja tylko do odczytu
        for (auto& f : functions) if (f.name == name) return &f;
        return nullptr;
    }
};

}
