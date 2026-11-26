#pragma once
// AST.h: wezly drzewa skladniowego (AST) - Expr (wyrazenia), Stmt (instrukcje),
// Param, Function, Program. To wynik etapu parsowania, zanim trafi do lowering.
// Brak petli w tym pliku - same deklaracje struktur danych.
#include "Types.h"
#include <memory>
#include <string>
#include <vector>

namespace scarlett {

struct Expr {
    enum Kind { IntLit, BoolLit, StrLit, Var, Binary, Unary, Call } kind;
    long long intVal = 0;          // wartosc dla IntLit
    bool boolVal = false;          // wartosc dla BoolLit
    std::string strVal;            // wartosc dla StrLit (tresc literalu napisowego)
    std::string name;              // nazwa zmiennej (Var) lub funkcji (Call)
    std::string op;                // operator dla Binary/Unary (np. "+", "!")
    std::shared_ptr<Expr> left, right, operand;
    std::vector<std::shared_ptr<Expr>> args;
};

struct Stmt {
    enum Kind { Block, VarDecl, Assign, If, While, DoWhile, For, Break, Continue, Return, ExprStmt } kind;
    ValueType type = ValueType::Int;
    std::string name;
    std::shared_ptr<Expr> init, cond, expr;
    std::shared_ptr<Expr> forPost;         // wyrazenie kroku petli for (po kazdej iteracji), moze byc puste
    std::shared_ptr<Stmt> thenStmt, elseStmt, body;
    std::shared_ptr<Stmt> forInit;         // instrukcja inicjujaca petli for (deklaracja lub przypisanie), moze byc pusta
    std::vector<std::shared_ptr<Stmt>> stmts;
};

struct Param { ValueType type; std::string name; };

struct Function {
    ValueType retType = ValueType::Int;
    std::string name;
    std::vector<Param> params;
    std::shared_ptr<Stmt> body;
    bool protect = false;
    std::string vmName;
    size_t sourceStart = 0;
    size_t sourceEnd = 0;
};

struct Program {
    std::vector<Function> functions;
};

}
