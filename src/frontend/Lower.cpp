// Lower.cpp: etap "lowering" - tlumaczy AST chronionych funkcji na IR
// (Lowerer::lowerExpr/lowerStmt/lowerFunction), przydzielajac rejestry
// zmiennym i generujac etykiety skokow dla if/while.
#include "scarlett/Lower.h"
#include "scarlett/IR.h"
#include <map>
#include <memory>

namespace scarlett {

namespace {
struct Lowerer {
    IRFunction fn;
    std::map<std::string,int> vars;
    int regCount = 0;
    int newReg() { return regCount++; }
    IRModule& mod;
    const std::map<std::string,std::string>& aliases;

    Lowerer(IRModule& m, const std::map<std::string,std::string>& a): mod(m), aliases(a) {}

    int lowerExpr(const std::shared_ptr<Expr>& e) {
        if (!e) return -1;
        if (e->kind == Expr::IntLit) {
            int r = newReg();
            IRInst i; i.op=IROp::LOADI; i.dst=r; i.imm=e->intVal; fn.code.push_back(i);
            return r;
        }
        if (e->kind == Expr::BoolLit) {
            int r = newReg();
            IRInst i; i.op=IROp::LOADI; i.dst=r; i.imm=e->boolVal?1:0; fn.code.push_back(i);
            return r;
        }
        if (e->kind == Expr::Var) {
            auto it = vars.find(e->name);
            if (it == vars.end()) {
                int r = newReg(); vars[e->name]=r;
                IRInst i; i.op=IROp::LOADI; i.dst=r; i.imm=0; fn.code.push_back(i);
                return r;
            }
            return it->second;
        }
        if (e->kind == Expr::Unary) {
            int v = lowerExpr(e->operand);
            int r = newReg();
            IRInst i;
            if (e->op == "-") { i.op=IROp::NEG; }
            else { i.op=IROp::NOT; }
            i.dst=r; i.a=v; fn.code.push_back(i);
            return r;
        }
        if (e->kind == Expr::Binary) {
            if (e->op == "=") {
                int rhs = lowerExpr(e->right);
                if (e->left && e->left->kind == Expr::Var) {
                    auto it = vars.find(e->left->name);
                    int vr;
                    if (it == vars.end()) { vr = newReg(); vars[e->left->name]=vr; }
                    else vr = it->second;
                    IRInst m; m.op=IROp::MOV; m.dst=vr; m.a=rhs; fn.code.push_back(m);
                    return vr;
                }
                return rhs;
            }
            int l = lowerExpr(e->left);
            int r = lowerExpr(e->right);
            int t = newReg();
            IRInst i;
            std::string op = e->op;
            if (op=="+") i.op=IROp::ADD;
            else if (op=="-") i.op=IROp::SUB;
            else if (op=="*") i.op=IROp::MUL;
            else if (op=="/") i.op=IROp::DIV;
            else if (op=="%") i.op=IROp::MOD;
            else if (op=="==") i.op=IROp::CMPEQ;
            else if (op=="!=") i.op=IROp::CMPNE;
            else if (op=="<") i.op=IROp::CMPLT;
            else if (op=="<=") i.op=IROp::CMPLE;
            else if (op==">") i.op=IROp::CMPGT;
            else if (op==">=") i.op=IROp::CMPGE;
            else if (op=="&&") i.op=IROp::AND;
            else if (op=="||") i.op=IROp::OR;
            else if (op=="&") i.op=IROp::AND;
            else if (op=="|") i.op=IROp::OR;
            else if (op=="^") i.op=IROp::XOR;
            else i.op=IROp::ADD;
            i.dst=t; i.a=l; i.b=r; fn.code.push_back(i);
            return t;
        }
        if (e->kind == Expr::Call) {
            std::vector<int> argRegs;
            // petla tlumaczy kolejne argumenty wywolania na rejestry
            for (auto& a : e->args) argRegs.push_back(lowerExpr(a));
            int dst = newReg();
            std::string callee = e->name;
            auto it = aliases.find(callee);
            if (it != aliases.end()) callee = it->second;
            IRInst i; i.op=IROp::CALL; i.dst=dst; i.func=callee; i.args=argRegs; fn.code.push_back(i);
            return dst;
        }
        int r=newReg();
        IRInst i; i.op=IROp::LOADI; i.dst=r; i.imm=0; fn.code.push_back(i);
        return r;
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
            if (s->init) {
                int v = lowerExpr(s->init);
                IRInst m; m.op=IROp::MOV; m.dst=r; m.a=v; fn.code.push_back(m);
            } else {
                IRInst i; i.op=IROp::LOADI; i.dst=r; i.imm=0; fn.code.push_back(i);
            }
            return;
        }
        if (s->kind == Stmt::Assign) {
            int rhs = lowerExpr(s->expr);
            auto it = vars.find(s->name);
            int vr;
            if (it == vars.end()) { vr = newReg(); vars[s->name]=vr; }
            else vr = it->second;
            IRInst m; m.op=IROp::MOV; m.dst=vr; m.a=rhs; fn.code.push_back(m);
            return;
        }
        if (s->kind == Stmt::ExprStmt) {
            if (s->expr) lowerExpr(s->expr);
            return;
        }
        if (s->kind == Stmt::If) {
            int c = lowerExpr(s->cond);
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
            int c = lowerExpr(s->cond);
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
                int c = lowerExpr(s->cond);
                IRInst bif; bif.op=IROp::JMPIF; bif.a=c; bif.label=Lbody; fn.code.push_back(bif);
                IRInst jmp; jmp.op=IROp::JMP; jmp.label=Lend; fn.code.push_back(jmp);
                IRInst labb; labb.op=IROp::LABEL; labb.label=Lbody; fn.code.push_back(labb);
            }
            lowerStmt(s->body);
            if (s->forPost) lowerExpr(s->forPost);
            IRInst jmp2; jmp2.op=IROp::JMP; jmp2.label=Lstart; fn.code.push_back(jmp2);
            IRInst labe; labe.op=IROp::LABEL; labe.label=Lend; fn.code.push_back(labe);
            return;
        }
        if (s->kind == Stmt::Return) {
            if (s->expr) {
                int v = lowerExpr(s->expr);
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
        regCount = 0;
        // petla przydziela rejestry kolejnym parametrom funkcji
        for (auto& p : f.params) {
            fn.params.push_back({p.type, p.name});
            vars[p.name] = regCount++;
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
    // petla tlumaczy kazda chroniona funkcje programu na funkcje IR
    for (auto& f : prog.functions) {
        if (!f.protect) continue;
        Lowerer L(mod, aliases);
        IRFunction irf = L.lowerFunction(f);
        mod.functions.push_back(irf);
    }
    return mod;
}
}
