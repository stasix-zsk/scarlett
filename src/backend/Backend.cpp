// Backend.cpp: tlumaczy modul IR na modul VM - mapuje opkody IROp na VMOp
// i zamienia etykiety skokow (JMP/JMPIF) oraz nazwy wywolan (CALL) na
// konkretne numeryczne adresy/indeksy w gotowym module VM.
#include "scarlett/Backend.h"
#include <map>

namespace scarlett {

static VMOp mapOp(IROp op) {
    switch (op) {
        case IROp::NOP: return VMOp::NOP;
        case IROp::LOADI: return VMOp::LOADI;
        case IROp::MOV: return VMOp::MOV;
        case IROp::ADD: return VMOp::ADD;
        case IROp::SUB: return VMOp::SUB;
        case IROp::MUL: return VMOp::MUL;
        case IROp::DIV: return VMOp::DIV;
        case IROp::MOD: return VMOp::MOD;
        case IROp::NEG: return VMOp::NEG;
        case IROp::CMPEQ: return VMOp::CMPEQ;
        case IROp::CMPNE: return VMOp::CMPNE;
        case IROp::CMPLT: return VMOp::CMPLT;
        case IROp::CMPLE: return VMOp::CMPLE;
        case IROp::CMPGT: return VMOp::CMPGT;
        case IROp::CMPGE: return VMOp::CMPGE;
        case IROp::AND: return VMOp::AND;
        case IROp::OR: return VMOp::OR;
        case IROp::XOR: return VMOp::XOR;
        case IROp::NOT: return VMOp::NOT;
        case IROp::JMP: return VMOp::JMP;
        case IROp::JMPIF: return VMOp::JMPIF;
        case IROp::CALL: return VMOp::CALL;
        case IROp::RET: return VMOp::RET;
        case IROp::RETV: return VMOp::RETV;
        case IROp::LOADS: return VMOp::LOADS;
        case IROp::SCAT: return VMOp::SCAT;
        case IROp::SLEN: return VMOp::SLEN;
        case IROp::SEQ: return VMOp::SEQ;
        case IROp::SNE: return VMOp::SNE;
        default: return VMOp::NOP;
    }
}

VMModule lowerToVM(const IRModule& ir) {
    VMModule mod;

    // petla tworzy puste funkcje VM (same metadane, bez kodu)
    for (auto& f : ir.functions) {
        VMFunction vf;
        vf.name = f.name;
        vf.retType = f.retType;
        vf.paramCount = (int)f.params.size();
        vf.regCount = f.regCount;
        vf.isProtected = f.isProtected;
        vf.strpool = f.strpool;
        mod.functions.push_back(vf);
    }

    // petla przepisuje instrukcje kazdej funkcji IR na instrukcje VM
    for (size_t fi=0; fi<ir.functions.size(); ++fi) {
        const IRFunction& f = ir.functions[fi];
        VMFunction& vf = mod.functions[fi];

        std::map<std::string,int> labels;
        int pos = 0;
        // petla liczy pozycje (indeks instrukcji) kazdej etykiety LABEL
        for (auto& ins : f.code) {
            if (ins.op == IROp::LABEL) {
                labels[ins.label] = pos;
            } else {
                ++pos;
            }
        }
        // petla zamienia instrukcje IR na VMInst, rozwiazujac etykiety i wywolania
        for (auto& ins : f.code) {
            if (ins.op == IROp::LABEL) continue;
            VMInst vi;
            vi.op = mapOp(ins.op);
            vi.dst = ins.dst;
            vi.a = ins.a;
            vi.b = ins.b;
            vi.imm = ins.imm;
            vi.args = ins.args;
            vi.func = ins.func;
            if (ins.op == IROp::JMP || ins.op == IROp::JMPIF) {
                auto it = labels.find(ins.label);
                if (it != labels.end()) vi.target = it->second;
                else vi.target = -1;
            }
            if (ins.op == IROp::CALL) {
                int idx = mod.indexOf(ins.func);
                vi.funcIndex = idx;
            }
            vf.code.push_back(vi);
        }
    }

    return mod;
}
}
