#pragma once
// Backend.h: pojedyncza funkcja lowerToVM, ktora tlumaczy modul IR na modul
// VM (zamienia etykiety skokow na adresy, mapuje opkody IR na opkody VM).
// Brak petli w tym pliku - sama deklaracja funkcji.
#include "IR.h"
#include "VM.h"

namespace scarlett {
VMModule lowerToVM(const IRModule& ir);
}
