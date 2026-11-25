// Runtime.cpp: interpreter bajtkodu VM (VM::run) z wlasnym stosem ramek
// wywolan (obsluguje rekurencje bez stosu C++) oraz scarlett_call_blob -
// funkcja wywolywana z .protected.cpp, dekodujaca i uruchamiajaca blob.
#include "scarlett/Runtime.h"
#include "scarlett/Codec.h"
#include <vector>

namespace scarlett {

namespace {
struct Frame {
    int funcIndex = -1;
    size_t pc = 0;
    std::vector<long long> regs;
    int callerFrame = -1;
    int dstReg = -1;
    bool hasDst = false;
};
}

namespace {
// Rdzen interpretera: wykonuje funkcje VM na wlasnym stosie ramek. Napisy zyja
// na "stercie" (heap) wspolnej dla calego wykonania - rejestr trzyma uchwyt
// (indeks w heap). Poczatkowa zawartosc heap pozwala przekazac napisy jako argumenty.
VM::Result executeModule(VMModule& mod, int funcIndex, const std::vector<long long>& args, std::vector<std::string> heap) {
    VM::Result res;
    if (funcIndex < 0 || funcIndex >= (int)mod.functions.size()) {
        res.ok = false; res.error = "zly indeks funkcji"; return res;
    }
    std::vector<Frame> stack;
    Frame root;
    root.funcIndex = funcIndex;
    root.pc = 0;
    root.regs.assign(mod.functions[funcIndex].regCount, 0);
    // petla kopiuje argumenty wywolania do rejestrow ramki glownej
    for (size_t i = 0; i < args.size() && i < root.regs.size(); ++i) root.regs[i] = args[i];
    root.hasDst = false;
    stack.push_back(root);

    // odczyt napisu spod uchwytu (zwraca kopie - bezpieczne wzgledem realokacji heap)
    auto handleStr = [&](long long h) -> std::string {
        if (h >= 0 && h < (long long)heap.size()) return heap[(size_t)h];
        return std::string();
    };

    long long retval = 0;
    bool done = false;
    // glowna petla interpretera: wykonuje kolejne instrukcje az do konca lub bledu
    while (!done && !stack.empty()) {
        Frame& f = stack.back();
        VMFunction& fn = mod.functions[f.funcIndex];
        if (f.pc >= fn.code.size()) {
            long long r = 0;
            int dst = f.dstReg; bool has = f.hasDst;
            stack.pop_back();
            if (stack.empty()) { retval = r; done = true; }
            else {
                Frame& cf = stack.back();
                if (has) cf.regs[dst] = r;
            }
            continue;
        }
        VMInst ins = fn.code[f.pc++];
        switch (ins.op) {
            case VMOp::NOP: break;
            case VMOp::LOADI:
                if (ins.dst >= 0 && ins.dst < (int)f.regs.size()) f.regs[ins.dst] = ins.imm;
                break;
            case VMOp::MOV:
                if (ins.dst>=0 && ins.a>=0 && ins.dst<(int)f.regs.size() && ins.a<(int)f.regs.size()) f.regs[ins.dst]=f.regs[ins.a];
                break;
            case VMOp::ADD: f.regs[ins.dst]=f.regs[ins.a]+f.regs[ins.b]; break;
            case VMOp::SUB: f.regs[ins.dst]=f.regs[ins.a]-f.regs[ins.b]; break;
            case VMOp::MUL: f.regs[ins.dst]=f.regs[ins.a]*f.regs[ins.b]; break;
            case VMOp::DIV:
                if (f.regs[ins.b]==0) { res.ok=false; res.error="dzielenie przez zero"; return res; }
                f.regs[ins.dst]=f.regs[ins.a]/f.regs[ins.b]; break;
            case VMOp::MOD:
                if (f.regs[ins.b]==0) { res.ok=false; res.error="modulo przez zero"; return res; }
                f.regs[ins.dst]=f.regs[ins.a]%f.regs[ins.b]; break;
            case VMOp::NEG: f.regs[ins.dst]= -f.regs[ins.a]; break;
            case VMOp::CMPEQ: f.regs[ins.dst]= (f.regs[ins.a]==f.regs[ins.b])?1:0; break;
            case VMOp::CMPNE: f.regs[ins.dst]= (f.regs[ins.a]!=f.regs[ins.b])?1:0; break;
            case VMOp::CMPLT: f.regs[ins.dst]= (f.regs[ins.a]<f.regs[ins.b])?1:0; break;
            case VMOp::CMPLE: f.regs[ins.dst]= (f.regs[ins.a]<=f.regs[ins.b])?1:0; break;
            case VMOp::CMPGT: f.regs[ins.dst]= (f.regs[ins.a]>f.regs[ins.b])?1:0; break;
            case VMOp::CMPGE: f.regs[ins.dst]= (f.regs[ins.a]>=f.regs[ins.b])?1:0; break;
            case VMOp::AND: f.regs[ins.dst]= (f.regs[ins.a]&&f.regs[ins.b])?1:0; break;
            case VMOp::OR: f.regs[ins.dst]= (f.regs[ins.a]||f.regs[ins.b])?1:0; break;
            case VMOp::XOR: f.regs[ins.dst]= (f.regs[ins.a]^f.regs[ins.b]); break;
            case VMOp::NOT: f.regs[ins.dst]= (!f.regs[ins.a])?1:0; break;
            case VMOp::JMP:
                f.pc = (size_t)ins.target; break;
            case VMOp::JMPIF:
                if (f.regs[ins.a] != 0) f.pc = (size_t)ins.target;
                break;
            case VMOp::CALL: {
                int callee = ins.funcIndex;
                if (callee < 0) callee = mod.indexOf(ins.func);
                if (callee < 0 || callee >= (int)mod.functions.size()) {
                    res.ok=false; res.error="nieznana funkcja "+ins.func; return res;
                }
                VMFunction& cf = mod.functions[callee];
                if (cf.isNative) {
                    // wywolanie funkcji natywnej - bez nowej ramki na stosie VM
                    std::vector<long long> a;
                    for (int r : ins.args) a.push_back(f.regs[r]);
                    long long r = cf.native ? cf.native(nullptr, a.data(), (int)a.size()) : 0;
                    if (ins.dst>=0 && ins.dst<(int)f.regs.size()) f.regs[ins.dst]=r;
                } else {
                    // wywolanie funkcji VM - nowa ramka na stosie (obsluga rekurencji)
                    Frame nf;
                    nf.funcIndex = callee;
                    nf.pc = 0;
                    nf.regs.assign(cf.regCount, 0);
                    // petla kopiuje argumenty wywolania do rejestrow nowej ramki
                    for (size_t i=0;i<ins.args.size() && i<nf.regs.size();++i) nf.regs[i]=f.regs[ins.args[i]];
                    nf.callerFrame = (int)stack.size()-1;
                    nf.dstReg = ins.dst;
                    nf.hasDst = (ins.dst>=0);
                    stack.push_back(nf);
                }
                break;
            }
            case VMOp::RET: {
                long long r = f.regs[ins.a];
                int dst = f.dstReg; bool has = f.hasDst;
                stack.pop_back();
                if (stack.empty()) { retval = r; done = true; }
                else {
                    Frame& cf = stack.back();
                    if (has && dst>=0 && dst<(int)cf.regs.size()) cf.regs[dst]=r;
                }
                break;
            }
            case VMOp::RETV: {
                stack.pop_back();
                if (stack.empty()) { retval = 0; done = true; }
                break;
            }
            case VMOp::LOADS: {
                // zaladuj literal napisowy z puli funkcji na sterte, uchwyt do rejestru
                const std::vector<std::string>& sp = fn.strpool;
                std::string v = (ins.imm >= 0 && ins.imm < (long long)sp.size()) ? sp[(size_t)ins.imm] : std::string();
                heap.push_back(std::move(v));
                if (ins.dst >= 0 && ins.dst < (int)f.regs.size()) f.regs[ins.dst] = (long long)(heap.size()-1);
                break;
            }
            case VMOp::SCAT: {
                // konkatenacja: policz wynik przed push_back (realokacja heap unieważnia referencje)
                std::string v = handleStr(f.regs[ins.a]);
                v += handleStr(f.regs[ins.b]);
                heap.push_back(std::move(v));
                if (ins.dst >= 0 && ins.dst < (int)f.regs.size()) f.regs[ins.dst] = (long long)(heap.size()-1);
                break;
            }
            case VMOp::SLEN:
                f.regs[ins.dst] = (long long)handleStr(f.regs[ins.a]).size(); break;
            case VMOp::SEQ:
                f.regs[ins.dst] = (handleStr(f.regs[ins.a]) == handleStr(f.regs[ins.b])) ? 1 : 0; break;
            case VMOp::SNE:
                f.regs[ins.dst] = (handleStr(f.regs[ins.a]) != handleStr(f.regs[ins.b])) ? 1 : 0; break;
            case VMOp::HALT:
                done = true; break;
            default:
                break;
        }
    }
    res.value = retval;
    res.heap = std::move(heap);
    return res;
}
}

VM::Result VM::run(VMModule& mod, int funcIndex, const std::vector<long long>& args) {
    return executeModule(mod, funcIndex, args, {});
}

VM::Result VM::run(VMModule& mod, const std::string& name, const std::vector<long long>& args) {
    return executeModule(mod, mod.indexOf(name), args, {});
}

VM::Result VM::runArgs(VMModule& mod, const std::string& name, const std::vector<Arg>& args) {
    // argumenty napisowe interowane na poczatkowej stercie; rejestr dostaje uchwyt
    std::vector<std::string> heap;
    std::vector<long long> a;
    for (const Arg& ar : args) {
        if (ar.isStr) { heap.push_back(ar.s); a.push_back((long long)(heap.size()-1)); }
        else a.push_back(ar.i);
    }
    return executeModule(mod, mod.indexOf(name), a, std::move(heap));
}

long long scarlett_call_blob(const unsigned char* data, size_t size, const char* funcName, const long long* args, int argc, int* ok) {
    std::vector<uint8_t> bytes(data, data + size);
    std::string err;
    bool integrity = false;
    scarlett::VMModule mod = scarlett::decodeModule(bytes, err, &integrity);
    if (!err.empty() || !integrity) {
        if (ok) *ok = 0;
        return 0;
    }
    std::vector<long long> a;
    // petla kopiuje surowe argumenty wywolania do wektora
    for (int i = 0; i < argc; ++i) a.push_back(args[i]);
    scarlett::VM vm;
    scarlett::VM::Result r = vm.run(mod, funcName, a);
    if (ok) *ok = r.ok ? 1 : 0;
    return r.value;
}

// Wspolny rdzen dla wariantow ABI obslugujacych napisy: dekoduje blob, buduje
// argumenty (int/string wg strs[k]!=nullptr) i uruchamia funkcje.
static VM::Result call_blob_args(const unsigned char* data, size_t size, const char* funcName,
                                 const long long* ints, const char* const* strs, int argc, int* ok) {
    std::vector<uint8_t> bytes(data, data + size);
    std::string err;
    bool integrity = false;
    VMModule mod = decodeModule(bytes, err, &integrity);
    VM::Result r;
    if (!err.empty() || !integrity) {
        if (ok) *ok = 0;
        r.ok = false;
        return r;
    }
    std::vector<VM::Arg> a;
    // petla buduje argumenty: napis gdy strs[i]!=nullptr, w przeciwnym razie liczba
    for (int i = 0; i < argc; ++i) {
        if (strs && strs[i]) a.push_back(VM::Arg(std::string(strs[i])));
        else a.push_back(VM::Arg((long long)(ints ? ints[i] : 0)));
    }
    VM vm;
    r = vm.runArgs(mod, funcName, a);
    if (ok) *ok = r.ok ? 1 : 0;
    return r;
}

long long scarlett_call_blob(const unsigned char* data, size_t size, const char* funcName, const long long* ints, const char* const* strs, int argc, int* ok) {
    return call_blob_args(data, size, funcName, ints, strs, argc, ok).value;
}

std::string scarlett_call_blob_str(const unsigned char* data, size_t size, const char* funcName, const long long* ints, const char* const* strs, int argc, int* ok) {
    return call_blob_args(data, size, funcName, ints, strs, argc, ok).str();
}
}
