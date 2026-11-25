#pragma once
// Runtime.h: klasa VM (interpreter bajtkodu) oraz scarlett_call_blob -
// funkcja wywolywana z wygenerowanego kodu .protected.cpp, ktora
// dekoduje blob, weryfikuje integralnosc i uruchamia wskazana funkcje.
// Brak petli w tym pliku - same deklaracje.
#include "VM.h"
#include <string>
#include <vector>

namespace scarlett {
class VM {
public:
    struct Result {
        bool ok = true;
        long long value = 0;            // dla funkcji int/bool: wynik; dla string: uchwyt do heap
        std::string error;
        std::vector<std::string> heap;  // sterta napisow z wykonania (uchwyt -> napis)

        // Odczytuje napis wskazany przez value (uchwyt). Dla funkcji zwracajacej
        // string zwraca jej wynik; dla pozostalych - pusty napis.
        std::string str() const {
            if (value >= 0 && value < (long long)heap.size()) return heap[(size_t)value];
            return std::string();
        }
    };

    // Argument wywolania VM: liczba calkowita albo napis. Pozwala przekazywac
    // parametry typu string do funkcji VM (napisy sa interowane na stercie).
    struct Arg {
        bool isStr = false;
        long long i = 0;
        std::string s;
        Arg(long long v) : isStr(false), i(v) {}
        Arg(int v) : isStr(false), i(v) {}
        Arg(const std::string& v) : isStr(true), s(v) {}
        Arg(const char* v) : isStr(true), s(v ? v : "") {}
    };

    Result run(VMModule& mod, int funcIndex, const std::vector<long long>& args);
    Result run(VMModule& mod, const std::string& name, const std::vector<long long>& args);
    // Wariant przyjmujacy argumenty mieszane (int/string) - napisy trafiaja na sterte.
    // Odrebna nazwa (nie przeciazenie run), by uniknac dwuznacznosci przy {liczby}.
    Result runArgs(VMModule& mod, const std::string& name, const std::vector<Arg>& args);
};

// ABI dla funkcji o sygnaturze wylacznie int/bool/void (bez napisow).
long long scarlett_call_blob(const unsigned char* data, size_t size, const char* funcName, const long long* args, int argc, int* ok);
// ABI dla funkcji, ktorych sygnatura zawiera napisy: ints[k]/strs[k] to wartosc
// k-tego argumentu (strs[k]!=nullptr oznacza argument napisowy). Pierwszy wariant
// zwraca wynik int/bool, drugi - napis (dla funkcji zwracajacych string).
long long scarlett_call_blob(const unsigned char* data, size_t size, const char* funcName, const long long* ints, const char* const* strs, int argc, int* ok);
std::string scarlett_call_blob_str(const unsigned char* data, size_t size, const char* funcName, const long long* ints, const char* const* strs, int argc, int* ok);
}
