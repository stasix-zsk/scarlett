#pragma once
// Runtime.h: klasa VM (interpreter bajtkodu) oraz scarlett_call_blob -
// funkcja wywolywana z wygenerowanego kodu .protected.cpp, ktora
// dekoduje blob, weryfikuje integralnosc i uruchamia wskazana funkcje.
// Brak petli w tym pliku - same deklaracje.
#include "VM.h"
#include <string>

namespace scarlett {
class VM {
public:
    struct Result {
        bool ok = true;
        long long value = 0;
        std::string error;
    };

    Result run(VMModule& mod, int funcIndex, const std::vector<long long>& args);
    Result run(VMModule& mod, const std::string& name, const std::vector<long long>& args);
};

long long scarlett_call_blob(const unsigned char* data, size_t size, const char* funcName, const long long* args, int argc, int* ok);
}
