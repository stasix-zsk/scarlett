// Parser.cpp: implementacja tokenizera i parsera prostego podzbioru C++
// (int/bool/void, arytmetyka, if/while, wywolania funkcji, return) -
// zamienia kod zrodlowy na drzewo AST zgodne z opisem w Parser.h.
#include "scarlett/Parser.h"
#include <cctype>
#include <stdexcept>

namespace scarlett {

static bool isIdentStart(char c){ return std::isalpha((unsigned char)c) || c=='_'; }
static bool isIdentChar(char c){ return std::isalnum((unsigned char)c) || c=='_'; }

Parser::Parser(const std::string& src): src_(src) { tokenize(); }

void Parser::tokenize() {
    size_t i = 0;
    // petla glowna: przechodzi przez caly kod zrodlowy znak po znaku
    while (i < src_.size()) {
        char c = src_[i];
        if (c==' '||c=='\t'||c=='\r'||c=='\n') { ++i; continue; }
        if (c=='/' && i+1<src_.size() && src_[i+1]=='/') { while (i<src_.size() && src_[i]!='\n') ++i; continue; } // pomija komentarz //
        if (c=='/' && i+1<src_.size() && src_[i+1]=='*') { i+=2; while (i+1<src_.size() && !(src_[i]=='*'&&src_[i+1]=='/')) ++i; i+=2; continue; } // pomija komentarz /* */
        Token t; t.pos=i;
        if (isIdentStart(c)) {
            size_t j=i; while (j<src_.size() && isIdentChar(src_[j])) ++j; // zbiera znaki identyfikatora
            t.type=Token::Ident; t.text=src_.substr(i,j-i); i=j;
            if (t.text=="int"||t.text=="bool"||t.text=="void"||t.text=="if"||t.text=="else"||t.text=="while"||t.text=="for"||t.text=="return"||t.text=="true"||t.text=="false") t.type=Token::Kw;
        } else if (std::isdigit((unsigned char)c)) {
            size_t j=i; while (j<src_.size() && std::isdigit((unsigned char)src_[j])) ++j; // zbiera cyfry liczby
            t.type=Token::Number; t.text=src_.substr(i,j-i); i=j;
        } else if (c=='"') {
            size_t j=i+1; std::string s; while (j<src_.size() && src_[j]!='"') { s+=src_[j]; ++j; } // zbiera znaki napisu
            t.type=Token::Str; t.text=s; i=j+1;
        } else {
            std::string two = src_.substr(i,2);
            if (two=="=="||two=="!="||two=="<="||two==">="||two=="&&"||two=="||"||two=="<<"||two==">>"||two=="::") { t.type=Token::Op; t.text=two; i+=2; }
            else if (c=='['||c==']'||c=='('||c==')'||c=='{'||c=='}'||c==';'||c==','||c=='=') { t.type=Token::Punct; t.text=std::string(1,c); i++; }
            else { t.type=Token::Op; t.text=std::string(1,c); i++; }
        }
        toks_.push_back(t);
    }
    Token eof; eof.type=Token::Eof; eof.text=""; eof.pos=src_.size(); toks_.push_back(eof);
}

Token& Parser::peek(size_t off){
    size_t idx=i_+off; if (idx>=toks_.size()) idx=toks_.size()-1; return toks_[idx];
}
Token Parser::next(){ if (i_<toks_.size()) return toks_[i_++]; return toks_.back(); }
bool Parser::match(const std::string& t){ if (i_<toks_.size() && toks_[i_].text==t){ ++i_; return true;} return false; }
bool Parser::matchType(Token::Type t){ if (i_<toks_.size() && toks_[i_].type==t){ ++i_; return true;} return false; }
void Parser::expect(const std::string& t){
    if (!match(t)) { if (error_.empty()) error_="expected "+t+" at pos "+std::to_string(peek().pos); }
}

ValueType Parser::parseType(){
    Token t=next();
    if (t.text=="int") return ValueType::Int;
    if (t.text=="bool") return ValueType::Bool;
    if (t.text=="void") return ValueType::Void;
    if (error_.empty()) error_="expected type";
    return ValueType::Int;
}

Program Parser::parse(){
    Program prog;
    // petla glowna: znajduje i parsuje kolejne definicje funkcji do konca pliku
    while (peek().type!=Token::Eof) {
        // petla pomija wszystko, co nie jest poczatkiem typu/atrybutu [[...]]
        while (peek().type!=Token::Eof && !(peek().text=="int"||peek().text=="bool"||peek().text=="void"||peek().text=="[")) {
            next();
        }
        if (peek().type==Token::Eof) break;
        bool protectAttr=false;

        // petla rozpoznaje atrybut C++ w stylu [[...]], np. [[vm_protect]]
        while (peek().text=="[" && i_+1<toks_.size() && toks_[i_+1].text=="[") {
            ++i_; ++i_;
            std::string content;
            // petla zbiera zawartosc atrybutu az do zamykajacego ]]
            while (!(peek().text=="]" && i_+1<toks_.size() && toks_[i_+1].text=="]") && peek().type!=Token::Eof) {
                content += peek().text; ++i_;
            }
            if (peek().text=="]") ++i_;
            if (peek().text=="]") ++i_;
            if (content.find("vm_protect")!=std::string::npos) protectAttr=true;
        }
        if (peek().type==Token::Eof) break;
        if (prog.functions.empty()) firstFuncPos_ = peek().pos;
        Function f = parseFunction(protectAttr);
        if (!error_.empty()) break;
        prog.functions.push_back(f);
    }
    return prog;
}

Function Parser::parseFunction(bool protectAttr){
    Function f;
    f.protect = protectAttr;
    f.sourceStart = peek().pos;
    f.retType = parseType();
    if (peek().type==Token::Ident) { f.name = next().text; }
    else { if(error_.empty()) error_="expected function name"; }
    expect("(");
    if (peek().text!=")") {
        // petla parsuje liste parametrow rozdzielonych przecinkami
        while (true) {
            Param p; p.type=parseType();
            if (peek().type==Token::Ident) p.name=next().text; else { if(error_.empty()) error_="param name"; }
            f.params.push_back(p);
            if (!match(",")) break;
        }
    }
    expect(")");
    while (peek().text!="{" && peek().type!=Token::Eof) next(); // pomija ewentualne specyfikatory przed cialem funkcji
    auto blk = parseBlock();
    f.body = blk;
    f.sourceEnd = peek().pos;
    return f;
}

std::shared_ptr<Stmt> Parser::parseBlock(){
    auto s = std::make_shared<Stmt>(); s->kind=Stmt::Block;
    expect("{");
    // petla parsuje kolejne instrukcje bloku az do klamry zamykajacej
    while (peek().text!="}" && peek().type!=Token::Eof) {
        s->stmts.push_back(parseStmt());
        if (!error_.empty()) return s;
    }
    expect("}");
    return s;
}

std::shared_ptr<Stmt> Parser::parseStmt(){
    if (peek().text=="if") {
        next();
        expect("(");
        auto c=parseExpr();
        expect(")");
        auto th=parseStmt();
        auto s=std::make_shared<Stmt>(); s->kind=Stmt::If; s->cond=c; s->thenStmt=th;
        if (peek().text=="else") { next(); s->elseStmt=parseStmt(); }
        return s;
    }
    if (peek().text=="while") {
        next();
        expect("(");
        auto c=parseExpr();
        expect(")");
        auto b=parseStmt();
        auto s=std::make_shared<Stmt>(); s->kind=Stmt::While; s->cond=c; s->body=b;
        return s;
    }
    if (peek().text=="for") {
        // for ( init ; cond ; post ) body
        // kazda z trzech klauzul moze byc pusta (np. for(;;) to nieskonczona petla)
        next();
        expect("(");
        auto s=std::make_shared<Stmt>(); s->kind=Stmt::For;
        // klauzula inicjujaca: deklaracja zmiennej (int/bool) albo wyrazenie, moze byc pusta
        if (peek().text!=";") {
            if (peek().text=="int"||peek().text=="bool"||peek().text=="void") {
                ValueType t=parseType();
                auto d=std::make_shared<Stmt>(); d->kind=Stmt::VarDecl; d->type=t;
                if (peek().type==Token::Ident) d->name=next().text;
                if (match("=")) d->init=parseExpr();
                s->forInit=d;
            } else {
                auto e=parseExpr();
                auto es=std::make_shared<Stmt>(); es->kind=Stmt::ExprStmt; es->expr=e;
                s->forInit=es;
            }
        }
        expect(";");
        // klauzula warunku: pusta oznacza warunek zawsze prawdziwy
        if (peek().text!=";") s->cond=parseExpr();
        expect(";");
        // klauzula kroku: wyrazenie wykonywane po kazdej iteracji, moze byc puste
        if (peek().text!=")") s->forPost=parseExpr();
        expect(")");
        s->body=parseStmt();
        return s;
    }
    if (peek().text=="return") {
        next();
        auto s=std::make_shared<Stmt>(); s->kind=Stmt::Return;
        if (peek().text!=";") s->expr=parseExpr();
        expect(";");
        return s;
    }
    if (peek().text=="{") return parseBlock();
    if (peek().text=="int"||peek().text=="bool"||peek().text=="void") {
        ValueType t=parseType();
        auto s=std::make_shared<Stmt>(); s->kind=Stmt::VarDecl; s->type=t;
        if (peek().type==Token::Ident) s->name=next().text;
        if (match("=")) s->init=parseExpr();
        expect(";");
        return s;
    }
    auto e=parseExpr();
    expect(";");
    auto s=std::make_shared<Stmt>(); s->kind=Stmt::ExprStmt; s->expr=e;
    return s;
}

std::shared_ptr<Expr> Parser::parseExpr(){ return parseAssign(); }

std::shared_ptr<Expr> Parser::parseAssign(){
    auto e=parseOr();
    if (peek().text=="=") {
        next();
        auto rhs=parseAssign();
        auto a=std::make_shared<Expr>(); a->kind=Expr::Binary; a->op="="; a->left=e; a->right=rhs;
        return a;
    }
    return e;
}
std::shared_ptr<Expr> Parser::parseOr(){
    auto e=parseAnd();
    // petla laczy kolejne skladniki operatorem || (lewostronnie)
    while (peek().text=="||") { next(); auto r=parseAnd(); auto b=std::make_shared<Expr>(); b->kind=Expr::Binary; b->op="||"; b->left=e; b->right=r; e=b; }
    return e;
}
std::shared_ptr<Expr> Parser::parseAnd(){
    auto e=parseEq();
    // petla laczy kolejne skladniki operatorem && (lewostronnie)
    while (peek().text=="&&") { next(); auto r=parseEq(); auto b=std::make_shared<Expr>(); b->kind=Expr::Binary; b->op="&&"; b->left=e; b->right=r; e=b; }
    return e;
}
std::shared_ptr<Expr> Parser::parseEq(){
    auto e=parseRel();
    // petla laczy kolejne skladniki operatorami == / != (lewostronnie)
    while (peek().text=="=="||peek().text=="!=") {
        std::string op=next().text; auto r=parseRel(); auto b=std::make_shared<Expr>(); b->kind=Expr::Binary; b->op=op; b->left=e; b->right=r; e=b;
    }
    return e;
}
std::shared_ptr<Expr> Parser::parseRel(){
    auto e=parseAdd();
    // petla laczy kolejne skladniki operatorami porownania (lewostronnie)
    while (peek().text=="<"||peek().text==">"||peek().text=="<="||peek().text==">=") {
        std::string op=next().text; auto r=parseAdd(); auto b=std::make_shared<Expr>(); b->kind=Expr::Binary; b->op=op; b->left=e; b->right=r; e=b;
    }
    return e;
}
std::shared_ptr<Expr> Parser::parseAdd(){
    auto e=parseMul();
    // petla laczy kolejne skladniki operatorami +, -, <<, >> (lewostronnie)
    while (peek().text=="+"||peek().text=="-"||peek().text=="<<"||peek().text==">>") {
        std::string op=next().text; auto r=parseMul(); auto b=std::make_shared<Expr>(); b->kind=Expr::Binary; b->op=op; b->left=e; b->right=r; e=b;
    }
    return e;
}
std::shared_ptr<Expr> Parser::parseMul(){
    auto e=parseUnary();
    // petla laczy kolejne skladniki operatorami *, /, % (lewostronnie)
    while (peek().text=="*"||peek().text=="/"||peek().text=="%") {
        std::string op=next().text; auto r=parseUnary(); auto b=std::make_shared<Expr>(); b->kind=Expr::Binary; b->op=op; b->left=e; b->right=r; e=b;
    }
    return e;
}
std::shared_ptr<Expr> Parser::parseUnary(){
    if (peek().text=="-"||peek().text=="!" ) {
        std::string op=next().text; auto e=parseUnary(); auto u=std::make_shared<Expr>(); u->kind=Expr::Unary; u->op=op; u->operand=e; return u;
    }
    return parsePrimary();
}
std::shared_ptr<Expr> Parser::parsePrimary(){
    if (peek().type==Token::Number) {
        auto e=std::make_shared<Expr>(); e->kind=Expr::IntLit; e->intVal=std::stoll(next().text); return e;
    }
    if (peek().type==Token::Str) {
        next();
        auto e=std::make_shared<Expr>(); e->kind=Expr::IntLit; e->intVal=0; return e;
    }
    if (peek().text=="true") { next(); auto e=std::make_shared<Expr>(); e->kind=Expr::BoolLit; e->boolVal=true; return e; }
    if (peek().text=="false") { next(); auto e=std::make_shared<Expr>(); e->kind=Expr::BoolLit; e->boolVal=false; return e; }
    if (peek().type==Token::Ident) {
        std::string name=next().text;
        // petla dokleja kolejne czlony nazwy po ::, . lub ->
        while (peek().text=="::" || peek().text=="." || peek().text=="->") {
            next();
            if (peek().type==Token::Ident) name += next().text;
            else break;
        }
        if (peek().text=="(") {
            next();
            auto e=std::make_shared<Expr>(); e->kind=Expr::Call; e->name=name;
            if (peek().text!=")") {
                // petla parsuje liste argumentow wywolania rozdzielonych przecinkami
                while (true) {
                    e->args.push_back(parseExpr());
                    if (!match(",")) break;
                }
            }
            expect(")");
            return e;
        }
        auto e=std::make_shared<Expr>(); e->kind=Expr::Var; e->name=name; return e;
    }
    if (peek().text=="(") {
        next(); auto e=parseExpr(); expect(")"); return e;
    }
    if (error_.empty()) error_="unexpected token "+peek().text;
    next();
    return std::make_shared<Expr>();
}

}
