#pragma once
// Lower.h: pojedyncza funkcja lowerProgram, ktora tlumaczy AST na IR dla
// funkcji oznaczonych jako chronione (protect == true).
// Brak petli w tym pliku - sama deklaracja funkcji.
#include "AST.h"
#include "IR.h"
#include <map>
#include <string>

namespace scarlett {
IRModule lowerProgram(const Program& prog, const std::map<std::string,std::string>& aliases = {});
}
