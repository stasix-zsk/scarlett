#pragma once
// Types.h: ValueType (int/bool/void) oraz typeName - zamiana typu na
// tekstowa nazwe (do generowania .protected.cpp i metadanych).
// Brak petli w tym pliku.
#include <string>

namespace scarlett {

enum class ValueType { Int, Bool, Void };

inline std::string typeName(ValueType t) {
    if (t == ValueType::Int) return "int";
    if (t == ValueType::Bool) return "bool";
    return "void";
}
}
