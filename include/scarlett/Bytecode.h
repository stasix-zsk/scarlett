#pragma once
// Bytecode.h: zestaw opkodow VMOp, pojedyncza instrukcja VMInst oraz
// VM_OP_COUNT (uzywane przy tasowaniu opkodow w Codec.cpp).
// Brak petli w tym pliku - same deklaracje danych.
#include "Types.h"
#include <cstdint>
#include <string>
#include <vector>

constexpr int VM_OP_COUNT = 30;

namespace scarlett {
enum class VMOp : uint8_t {
    NOP=0, LOADI, MOV,
    ADD,SUB,MUL,DIV,MOD,
    NEG,
    CMPEQ,CMPNE,CMPLT,CMPLE,CMPGT,CMPGE,
    AND,OR,XOR,NOT,
    JMP,JMPIF,
    CALL,RET,RETV,
    LOADS,      // zaladuj literal napisowy z puli stringow funkcji (imm = indeks w puli)
    SCAT,       // konkatenacja: dst = str(a) + str(b)
    SLEN,       // dlugosc: dst = len(str(a))  (wynik int)
    SEQ,        // rownosc napisow: dst = (str(a)==str(b)) ? 1 : 0
    SNE,        // nierownosc napisow: dst = (str(a)!=str(b)) ? 1 : 0
    HALT,
    COUNT
};

struct VMInst {
    VMOp op = VMOp::NOP;
    int dst = -1, a = -1, b = -1;   // indeksy rejestrow docelowego/zrodlowych
    long long imm = 0;              // wartosc natychmiastowa (dla LOADI)
    int target = -1;                // docelowy adres instrukcji (dla JMP/JMPIF)
    std::string func;               // nazwa funkcji (dla CALL, gdy funcIndex nieznany)
    std::vector<int> args;          // rejestry z argumentami wywolania (dla CALL)
    int funcIndex = -1;             // indeks funkcji w module (dla CALL)
};

inline int vmOpCount() { return (int)VMOp::COUNT; }
}
