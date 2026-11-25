// Lower.cpp: etap "lowering" - tlumaczy AST chronionych funkcji na IR
// (Lowerer::lowerExpr/lowerStmt/lowerFunction), przydzielajac rejestry
// zmiennym i generujac etykiety skokow dla if/while.
#include "scarlett/Lower.h"
#include "scarlett/IR.h"
#include <map>
#include <memory>

namespace scarlett {

namespace {
// Wynik lowerowania wyrazenia: rejestr z wartoscia + jej typ statyczny.
// Typ pozwala wybrac wlasciwy opkod dla napisow (SCAT/SEQ/SNE zamiast ADD/CMP*).
struct LRes { int reg = -1; ValueType type = ValueType::Int; };

struct Lowerer {
    IRFunction fn;
    std::map<std::string,int> vars;
    std::map<std::string,ValueType> varTypes;   // typ kazdej zmiennej (do inferencji napisow)
    std::map<std::string,int> strlits;          // deduplikacja literalow w puli stringow funkcji
    int regCount = 0;
    int newReg() { return regCount++; }
    IRModule& mod;
    const std::map<std::string,std::string>& aliases;
    const std::map<std::string,ValueType>& funcRet;  // typ zwracany funkcji (po nazwie zrodlowej)

    Lowerer(IRModule& m, const std::map<std::string,std::string>& a, const std::map<std::string,ValueType>& fr)
        : mod(m), aliases(a), funcRet(fr) {}

    // Interuje literal napisowy w puli funkcji, zwracajac jego indeks.
    int internStr(const std::string& s) {
        auto it = strlits.find(s);
        if (it != strlits.end()) return it->second;
        int idx = (int)fn.strpool.size();
        fn.strpool.push_back(s);
        strlits[s] = idx;
        return idx;
    }

    LRes lowerExpr(const std::shared_ptr<Expr>& e) {
        if (!e) return {-1, ValueType::Int};
        if (e->kind == Expr::IntLit) {
            int r = newReg();
            IRInst i; i.op=IROp::LOADI; i.dst=r; i.imm=e->intVal; fn.code.push_back(i);
            return {r, ValueType::Int};
        }
        if (e->kind == Expr::BoolLit) {
            int r = newReg();
            IRInst i; i.op=IROp::LOADI; i.dst=r; i.imm=e->boolVal?1:0; fn.code.push_back(i);
            return {r, ValueType::Bool};
        }
        if (e->kind == Expr::StrLit) {
            int r = newReg();
            IRInst i; i.op=IROp::LOADS; i.dst=r; i.imm=internStr(e->strVal); fn.code.push_back(i);
            return {r, ValueType::Str};
        }
        if (e->kind == Expr::Var) {
            auto it = vars.find(e->name);
            if (it == vars.end()) {
                int r = newReg(); vars[e->name]=r; varTypes[e->name]=ValueType::Int;
                IRInst i; i.op=IROp::LOADI; i.dst=r; i.imm=0; fn.code.push_back(i);
                return {r, ValueType::Int};
            }
            ValueType t = ValueType::Int;
            auto vt = varTypes.find(e->name); if (vt != varTypes.end()) t = vt->second;
            return {it->second, t};
        }
        if (e->kind == Expr::Unary) {
            LRes v = lowerExpr(e->operand);
            int r = newReg();
            IRInst i;
            if (e->op == "-") { i.op=IROp::NEG; }
            else { i.op=IROp::NOT; }
            i.dst=r; i.a=v.reg; fn.code.push_back(i);
            return {r, e->op=="-" ? ValueType::Int : ValueType::Bool};
        }
        if (e->kind == Expr::Binary) {
            if (e->op == "=") {
                LRes rhs = lowerExpr(e->right);
                if (e->left && e->left->kind == Expr::Var) {
                    auto it = vars.find(e->left->name);
                    int vr;
                    if (it == vars.end()) { vr = newReg(); vars[e->left->name]=vr; varTypes[e->left->name]=rhs.type; }
                    else vr = it->second;
                    IRInst m; m.op=IROp::MOV; m.dst=vr; m.a=rhs.reg; fn.code.push_back(m);
                    ValueType t = rhs.type;
                    auto vt = varTypes.find(e->left->name); if (vt != varTypes.end()) t = vt->second;
                    return {vr, t};
                }
                return rhs;
            }
            LRes L = lowerExpr(e->left);
            LRes R = lowerExpr(e->right);
            int t = newReg();
            IRInst i; i.dst=t; i.a=L.reg; i.b=R.reg;
            std::string op = e->op;
            bool str = (L.type==ValueType::Str || R.type==ValueType::Str);
            ValueType rt = ValueType::Int;
            // operatory napisowe: + (konkatenacja), == / != (porownanie)
            if (str && op=="+") { i.op=IROp::SCAT; rt=ValueType::Str; }
            else if (str && op=="==") { i.op=IROp::SEQ; rt=ValueType::Bool; }
            else if (str && op=="!=") { i.op=IROp::SNE; rt=ValueType::Bool; }
            else if (op=="+") i.op=IROp::ADD;
            else if (op=="-") i.op=IROp::SUB;
            else if (op=="*") i.op=IROp::MUL;
            else if (op=="/") i.op=IROp::DIV;
            else if (op=="%") i.op=IROp::MOD;
            else if (op=="==") { i.op=IROp::CMPEQ; rt=ValueType::Bool; }
            else if (op=="!=") { i.op=IROp::CMPNE; rt=ValueType::Bool; }
            else if (op=="<")  { i.op=IROp::CMPLT; rt=ValueType::Bool; }
            else if (op=="<=") { i.op=IROp::CMPLE; rt=ValueType::Bool; }
            else if (op==">")  { i.op=IROp::CMPGT; rt=ValueType::Bool; }
            else if (op==">=") { i.op=IROp::CMPGE; rt=ValueType::Bool; }
            else if (op=="&&") { i.op=IROp::AND; rt=ValueType::Bool; }
            else if (op=="||") { i.op=IROp::OR; rt=ValueType::Bool; }
            else if (op=="&") i.op=IROp::AND;
            else if (op=="|") i.op=IROp::OR;
            else if (op=="^") i.op=IROp::XOR;
            else i.op=IROp::ADD;
            fn.code.push_back(i);
            return {t, rt};
        }
        if (e->kind == Expr::Call) {
            // wbudowane len(s)/length(s)/size(s): dlugosc napisu -> int
            if ((e->name=="len" || e->name=="length" || e->name=="size") && e->args.size()==1) {
                LRes a = lowerExpr(e->args[0]);
                int r = newReg();
                IRInst i; i.op=IROp::SLEN; i.dst=r; i.a=a.reg; fn.code.push_back(i);
                return {r, ValueType::Int};
            }
            std::vector<int> argRegs;
            // petla tlumaczy kolejne argumenty wywolania na rejestry
            for (auto& a : e->args) argRegs.push_back(lowerExpr(a).reg);
            int dst = newReg();
            std::string callee = e->name;
            auto it = aliases.find(callee);
            if (it != aliases.end()) callee = it->second;
            IRInst i; i.op=IROp::CALL; i.dst=dst; i.func=callee; i.args=argRegs; fn.code.push_back(i);
            ValueType rt = ValueType::Int;
            auto ct = funcRet.find(e->name); if (ct != funcRet.end()) rt = ct->second;
            return {dst, rt};
        }
        int r=newReg();
        IRInst i; i.op=IROp::LOADI; i.dst=r; i.imm=0; fn.code.push_back(i);
        return {r, ValueType::Int};
    }

    void lowerStmt(const std::shared_ptr<Stmt>& s) {
        if (!s) return;
        if (s->kind == Stmt::Block) {
            // petla tlumaczy kolejne instrukcje bloku
            for (auto& st : s->stmts) lowerStmt(st);
            return;
        }
        if (s->kind == Stmt::VarDecl) {
            int r = newReg();
            vars[s->name]=r;
            varTypes[s->name]=s->type;
            if (s->init) {
                LRes v = lowerExpr(s->init);
                IRInst m; m.op=IROp::MOV; m.dst=r; m.a=v.reg; fn.code.push_back(m);
            } else if (s->type == ValueType::Str) {
                // niezainicjalizowany napis = pusty napis (poprawny uchwyt na stercie)
                IRInst i; i.op=IROp::LOADS; i.dst=r; i.imm=internStr(""); fn.code.push_back(i);
            } else {
                IRInst i; i.op=IROp::LOADI; i.dst=r; i.imm=0; fn.code.push_back(i);
            }
            return;
        }
        if (s->kind == Stmt::Assign) {
            LRes rhs = lowerExpr(s->expr);
            auto it = vars.find(s->name);
            int vr;
            if (it == vars.end()) { vr = newReg(); vars[s->name]=vr; varTypes[s->name]=rhs.type; }
            else vr = it->second;
            IRInst m; m.op=IROp::MOV; m.dst=vr; m.a=rhs.reg; fn.code.push_back(m);
            return;
        }
        if (s->kind == Stmt::ExprStmt) {
            if (s->expr) lowerExpr(s->expr);
            return;
        }
        if (s->kind == Stmt::If) {
            int c = lowerExpr(s->cond).reg;
            std::string Lthen = "L"+std::to_string(fn.code.size())+"t";
            std::string Lend = "L"+std::to_string(fn.code.size())+"e";
            IRInst bif; bif.op=IROp::JMPIF; bif.a=c; bif.label=Lthen; fn.code.push_back(bif);
            if (s->elseStmt) lowerStmt(s->elseStmt);
            IRInst jmp; jmp.op=IROp::JMP; jmp.label=Lend; fn.code.push_back(jmp);
            IRInst lab; lab.op=IROp::LABEL; lab.label=Lthen; fn.code.push_back(lab);
            lowerStmt(s->thenStmt);
            IRInst lab2; lab2.op=IROp::LABEL; lab2.label=Lend; fn.code.push_back(lab2);
            return;
        }
        if (s->kind == Stmt::While) {
            std::string Lstart = "L"+std::to_string(fn.code.size())+"s";
            std::string Lbody = "L"+std::to_string(fn.code.size())+"b";
            std::string Lend = "L"+std::to_string(fn.code.size())+"w";
            IRInst lab; lab.op=IROp::LABEL; lab.label=Lstart; fn.code.push_back(lab);
            int c = lowerExpr(s->cond).reg;
            IRInst bif; bif.op=IROp::JMPIF; bif.a=c; bif.label=Lbody; fn.code.push_back(bif);
            IRInst jmp; jmp.op=IROp::JMP; jmp.label=Lend; fn.code.push_back(jmp);
            IRInst labb; labb.op=IROp::LABEL; labb.label=Lbody; fn.code.push_back(labb);
            lowerStmt(s->body);
            IRInst jmp2; jmp2.op=IROp::JMP; jmp2.label=Lstart; fn.code.push_back(jmp2);
            IRInst labe; labe.op=IROp::LABEL; labe.label=Lend; fn.code.push_back(labe);
            return;
        }
        if (s->kind == Stmt::For) {
            // for desugaruje sie do: init ; start: if(cond) goto body else goto end ;
            //                        body: <cialo> ; <krok> ; goto start ; end:
            if (s->forInit) lowerStmt(s->forInit);
            std::string Lstart = "L"+std::to_string(fn.code.size())+"fs";
            std::string Lbody  = "L"+std::to_string(fn.code.size())+"fb";
            std::string Lend   = "L"+std::to_string(fn.code.size())+"fe";
            IRInst lab; lab.op=IROp::LABEL; lab.label=Lstart; fn.code.push_back(lab);
            if (s->cond) {
                // pusty warunek (for(;;)) = petla bez testu, wychodzi tylko przez return
                int c = lowerExpr(s->cond).reg;
                IRInst bif; bif.op=IROp::JMPIF; bif.a=c; bif.label=Lbody; fn.code.push_back(bif);
                IRInst jmp; jmp.op=IROp::JMP; jmp.label=Lend; fn.code.push_back(jmp);
                IRInst labb; labb.op=IROp::LABEL; labb.label=Lbody; fn.code.push_back(labb);
            }
            lowerStmt(s->body);
            if (s->forPost) lowerExpr(s->forPost);
            IRInst jmp2f; jmp2f.op=IROp::JMP; jmp2f.label=Lstart; fn.code.push_back(jmp2f);
            IRInst labe; labe.op=IROp::LABEL; labe.label=Lend; fn.code.push_back(labe);
            return;
        }
        if (s->kind == Stmt::Return) {
            if (s->expr) {
                int v = lowerExpr(s->expr).reg;
                IRInst r; r.op=IROp::RET; r.a=v; fn.code.push_back(r);
            } else {
                IRInst r; r.op=IROp::RETV; fn.code.push_back(r);
            }
            return;
        }
    }

    IRFunction lowerFunction(const Function& f) {
        fn = IRFunction();
        std::string vmn = f.vmName.empty() ? f.name : f.vmName;
        fn.name = vmn;
        fn.vmName = vmn;
        fn.retType = f.retType;
        fn.isProtected = f.protect;
        vars.clear();
        varTypes.clear();
        strlits.clear();
        regCount = 0;
        // petla przydziela rejestry kolejnym parametrom funkcji
        for (auto& p : f.params) {
            fn.params.push_back({p.type, p.name});
            vars[p.name] = regCount++;
            varTypes[p.name] = p.type;
        }
        if (f.body) lowerStmt(f.body);
        if (fn.code.empty() || (fn.code.back().op != IROp::RET && fn.code.back().op != IROp::RETV)) {
            if (fn.retType == ValueType::Void) { IRInst r; r.op=IROp::RETV; fn.code.push_back(r); }
            else { IRInst r; r.op=IROp::LOADI; r.dst=newReg(); r.imm=0; fn.code.push_back(r); IRInst rr; rr.op=IROp::RET; rr.a=fn.code.back().dst; fn.code.push_back(rr); }
        }
        fn.regCount = regCount;
        if (fn.regCount < (int)fn.params.size()) fn.regCount = (int)fn.params.size();
        return fn;
    }
};

}

IRModule lowerProgram(const Program& prog, const std::map<std::string,std::string>& aliases) {
    IRModule mod;
    // mapa typow zwracanych (po nazwie zrodlowej funkcji) - do inferencji typu
    // wyniku wywolania (np. konkatenacja wyniku funkcji zwracajacej string).
    std::map<std::string,ValueType> funcRet;
    for (auto& f : prog.functions) funcRet[f.name] = f.retType;
    // petla tlumaczy kazda chroniona funkcje programu na funkcje IR
    for (auto& f : prog.functions) {
        if (!f.protect) continue;
        Lowerer L(mod, aliases, funcRet);
        IRFunction irf = L.lowerFunction(f);
        mod.functions.push_back(irf);
    }
    return mod;
}
}
