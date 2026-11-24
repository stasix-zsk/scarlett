#pragma once
// Parser.h: Token oraz klasa Parser, ktora tokenizuje i parsuje prosty
// podzbior C++ (int/bool/void, arytmetyka, if/while, wywolania, return)
// do drzewa AST. Brak petli w tym pliku - same deklaracje.
#include "AST.h"
#include <string>
#include <vector>

namespace scarlett {

struct Token {
    enum Type { Kw, Ident, Number, Str, Op, Punct, Eof } type;
    std::string text;
    size_t pos = 0;  // pozycja w oryginalnym zrodle (do wyciecia kodu niechronionych funkcji)
};

class Parser {
public:
    Parser(const std::string& src);
    Program parse();
    const std::string& error() const { return error_; }
    size_t firstFunctionPos() const { return firstFuncPos_; }
private:
    std::string src_;
    std::vector<Token> toks_;
    size_t i_ = 0;              // indeks aktualnie przetwarzanego tokenu
    std::string error_;
    size_t firstFuncPos_ = 0;

    void tokenize();
    Token& peek(size_t off=0);
    Token next();
    bool match(const std::string& t);
    bool matchType(Token::Type t);
    void expect(const std::string& t);

    Function parseFunction(bool protectAttr);
    ValueType parseType();
    std::shared_ptr<Stmt> parseStmt();
    std::shared_ptr<Stmt> parseBlock();
    std::shared_ptr<Expr> parseExpr();
    std::shared_ptr<Expr> parseAssign();
    std::shared_ptr<Expr> parseOr();
    std::shared_ptr<Expr> parseAnd();
    std::shared_ptr<Expr> parseEq();
    std::shared_ptr<Expr> parseRel();
    std::shared_ptr<Expr> parseAdd();
    std::shared_ptr<Expr> parseMul();
    std::shared_ptr<Expr> parseUnary();
    std::shared_ptr<Expr> parsePrimary();
};

}
