#pragma once
// Types.h: ValueType (int/bool/void) oraz typeName - zamiana typu na
// tekstowa nazwe (do generowania .protected.cpp i metadanych).
// Brak petli w tym pliku.
#include <string>

namespace scarlett {

enum class ValueType { Int, Bool, Void, Str };

inline std::string typeName(ValueType t) {
    if (t == ValueType::Int) return "int";
    if (t == ValueType::Bool) return "bool";
    if (t == ValueType::Str) return "string";
    return "void";
}
}
