#pragma once
// Codec.h: EncodeResult (zaszyfrowane bajty + klucze) oraz funkcje
// encodeModule/decodeModule do (de)szyfrowania modulu VM.
// Brak petli w tym pliku - same deklaracje.
#include "VM.h"
#include <cstdint>
#include <string>
#include <vector>

namespace scarlett {

struct EncodeResult {
    std::vector<uint8_t> bytes;      // zakodowane (zaszyfrowane) bajty modulu
    uint64_t seed = 0;                // ziarno generatora liczb losowych
    uint32_t constKey = 0;            // klucz XOR dla wartosci natychmiastowych/adresow
    uint32_t blockKey = 0;            // klucz XOR dla calego bloku instrukcji
    std::vector<uint8_t> opcodeMap;   // permutacja (tasowanie) opkodow
};

EncodeResult encodeModule(const VMModule& mod, uint64_t seed);
VMModule decodeModule(const std::vector<uint8_t>& bytes, std::string& error, bool* integrityOk = nullptr);

}
