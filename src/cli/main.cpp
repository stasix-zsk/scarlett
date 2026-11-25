// main.cpp: narzedzie CLI "scarlett" - wczytuje plik .cpp, parsuje go,
// oznacza wskazane funkcje jako chronione, tlumaczy je AST->IR->bajtkod,
// szyfruje i zapisuje .scarlett/.protected.cpp/.meta w katalogu wyjsciowym.
#include "scarlett/Parser.h"
#include "scarlett/Lower.h"
#include "scarlett/Backend.h"
#include "scarlett/Codec.h"
#include "scarlett/VM.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <random>
#include <map>

namespace fs = std::filesystem;

static std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss; ss << f.rdbuf(); return ss.str();
}

static void writeFile(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary);
    f << content;
}

static std::vector<std::string> split(const std::string& s, char d) {
    std::vector<std::string> out;
    std::string cur;
    // petla rozbija napis na fragmenty wzgledem separatora d
    for (char c : s) {
        if (c == d) { if (!cur.empty()) out.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

static std::string typeStr(scarlett::ValueType t) {
    using scarlett::ValueType;
    if (t == ValueType::Int) return "int";
    if (t == ValueType::Bool) return "bool";
    if (t == ValueType::Str) return "std::string";
    return "void";
}

int main(int argc, char** argv) {
    std::string input, outdir = ".", protectList;
    uint64_t seed = 0;
    // petla odczytuje argumenty wiersza polecen (--input/--outdir/--protect/--seed)
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--input" || a == "-i") { input = argv[++i]; }
        else if (a == "--outdir" || a == "-o") { outdir = argv[++i]; }
        else if (a == "--protect" || a == "-p") { protectList = argv[++i]; }
        else if (a == "--seed") { seed = std::stoull(argv[++i]); }
    }
    if (input.empty()) {
        std::cerr << "uzycie: scarlett --input plik.cpp [--protect f1,f2] [--outdir katalog] [--seed N]\n";
        return 1;
    }
    if (seed == 0) {
        std::random_device rd;
        seed = ((uint64_t)rd() << 32) | rd();
    }

    std::string src = readFile(input);
    scarlett::Parser parser(src);
    scarlett::Program prog = parser.parse();
    if (!parser.error().empty()) {
        std::cerr << "blad parsowania: " << parser.error() << "\n";
        return 1;
    }

    std::vector<std::string> protectNames = split(protectList, ',');
    // petla oznacza funkcje wymienione w --protect jako chronione
    for (auto& f : prog.functions) {
        if (std::find(protectNames.begin(), protectNames.end(), f.name) != protectNames.end())
            f.protect = true;
    }

    std::mt19937 rng((uint32_t)seed);
    auto randIdent = [&]() {
        static const char* pool[] = {"a","b","x","y","z","w","t","u","v","r","s","n","m","p","q","wyn","tmp","val","res","lic","idx","obr","prc","fnc"};
        std::string r = pool[rng() % (sizeof(pool)/sizeof(pool[0]))];
        r += std::to_string(rng() % 1000);
        return r;
    };
    std::map<std::string,std::string> aliases;
    // petla losuje unikalny alias (ukryta nazwe w VM) dla kazdej chronionej funkcji
    for (auto& f : prog.functions) {
        if (!f.protect) continue;
        std::string alias = "f" + std::to_string(rng() % 10000);
        while (aliases.count(alias)) alias = "f" + std::to_string(rng() % 10000); // szuka nazwy jeszcze nie uzytej
        f.vmName = alias;
        aliases[f.name] = alias;
    }

    scarlett::IRModule ir = scarlett::lowerProgram(prog, aliases);
    scarlett::VMModule vm = scarlett::lowerToVM(ir);
    scarlett::EncodeResult enc = scarlett::encodeModule(vm, seed);

    fs::create_directories(outdir);
    std::string base = fs::path(input).stem().string();

    {
        std::ofstream f(outdir + "/" + base + ".scarlett", std::ios::binary);
        f.write((const char*)enc.bytes.data(), (std::streamsize)enc.bytes.size());
    }

    std::ostringstream cpp;
    cpp << "#include \"scarlett/Runtime.h\"\n";
    cpp << "#include <cstddef>\n";
    cpp << "#include <cstdlib>\n";
    cpp << "#include <string>\n\n";

    std::string blobVar = randIdent();
    cpp << "static const unsigned char " << blobVar << "[] = {\n";
    // petla zapisuje zaszyfrowane bajty bloku jako literaly hex, 12 na linie
    for (size_t i = 0; i < enc.bytes.size(); ++i) {
        if (i % 12 == 0) cpp << "    ";
        cpp << "0x" << std::hex << (int)enc.bytes[i] << std::dec;
        if (i + 1 < enc.bytes.size()) cpp << ",";
        if (i % 12 == 11) cpp << "\n";
    }
    cpp << "\n};\n\n";

    // zachowujemy fragment pliku przed pierwsza funkcja (np. include'y)
    cpp << src.substr(0, parser.firstFunctionPos());

    // petla generuje kod kazdej funkcji: trampoline dla chronionych, oryginal dla reszty
    for (auto& f : prog.functions) {
        if (f.protect) {
            // czy sygnatura dotyka napisow? decyduje o wyborze ABI
            using scarlett::ValueType;
            bool sigHasStr = (f.retType == ValueType::Str);
            for (auto& pr : f.params) if (pr.type == ValueType::Str) sigHasStr = true;

            // chroniona funkcja: trampolina wywoluje scarlett_call_blob, oryginalne cialo znika
            cpp << typeStr(f.retType) << " " << f.name << "(";
            // petla zapisuje liste parametrow w sygnaturze trampoliny
            for (size_t p = 0; p < f.params.size(); ++p) {
                if (p) cpp << ", ";
                cpp << typeStr(f.params[p].type) << " " << f.params[p].name;
            }
            cpp << ") {\n";
            std::string vOk = randIdent();
            std::string vRes = randIdent();
            std::string callName = f.vmName.empty() ? f.name : f.vmName;

            if (!sigHasStr) {
                // sygnatura wylacznie int/bool/void: stare ABI (long long)
                std::string vArgs = randIdent();
                cpp << "    long long " << vArgs << "[] = {";
                for (size_t p = 0; p < f.params.size(); ++p) { if (p) cpp << ","; cpp << f.params[p].name; }
                if (f.params.empty()) cpp << "0";
                cpp << "};\n";
                cpp << "    int " << vOk << " = 1;\n";
                if (f.retType == ValueType::Void) {
                    cpp << "    (void)scarlett::scarlett_call_blob(" << blobVar << ", sizeof(" << blobVar << "), \"" << callName << "\", " << vArgs << ", " << f.params.size() << ", &" << vOk << ");\n";
                    cpp << "    if (!" << vOk << ") std::abort();\n";
                    cpp << "}\n\n";
                } else {
                    cpp << "    long long " << vRes << " = scarlett::scarlett_call_blob(" << blobVar << ", sizeof(" << blobVar << "), \"" << callName << "\", " << vArgs << ", " << f.params.size() << ", &" << vOk << ");\n";
                    cpp << "    if (!" << vOk << ") std::abort();\n";
                    cpp << "    return (" << typeStr(f.retType) << ")" << vRes << ";\n";
                    cpp << "}\n\n";
                }
            } else {
                // sygnatura zawiera string: ABI przekazujace rownolegle tablice ints[]/strs[]
                std::string vInts = randIdent();
                std::string vStrs = randIdent();
                cpp << "    long long " << vInts << "[] = {";
                for (size_t p = 0; p < f.params.size(); ++p) {
                    if (p) cpp << ",";
                    cpp << (f.params[p].type == ValueType::Str ? std::string("0") : f.params[p].name);
                }
                if (f.params.empty()) cpp << "0";
                cpp << "};\n";
                cpp << "    const char* " << vStrs << "[] = {";
                for (size_t p = 0; p < f.params.size(); ++p) {
                    if (p) cpp << ",";
                    cpp << (f.params[p].type == ValueType::Str ? (f.params[p].name + ".c_str()") : std::string("nullptr"));
                }
                if (f.params.empty()) cpp << "nullptr";
                cpp << "};\n";
                cpp << "    int " << vOk << " = 1;\n";
                if (f.retType == ValueType::Str) {
                    cpp << "    std::string " << vRes << " = scarlett::scarlett_call_blob_str(" << blobVar << ", sizeof(" << blobVar << "), \"" << callName << "\", " << vInts << ", " << vStrs << ", " << f.params.size() << ", &" << vOk << ");\n";
                    cpp << "    if (!" << vOk << ") std::abort();\n";
                    cpp << "    return " << vRes << ";\n";
                    cpp << "}\n\n";
                } else if (f.retType == ValueType::Void) {
                    cpp << "    (void)scarlett::scarlett_call_blob(" << blobVar << ", sizeof(" << blobVar << "), \"" << callName << "\", " << vInts << ", " << vStrs << ", " << f.params.size() << ", &" << vOk << ");\n";
                    cpp << "    if (!" << vOk << ") std::abort();\n";
                    cpp << "}\n\n";
                } else {
                    cpp << "    long long " << vRes << " = scarlett::scarlett_call_blob(" << blobVar << ", sizeof(" << blobVar << "), \"" << callName << "\", " << vInts << ", " << vStrs << ", " << f.params.size() << ", &" << vOk << ");\n";
                    cpp << "    if (!" << vOk << ") std::abort();\n";
                    cpp << "    return (" << typeStr(f.retType) << ")" << vRes << ";\n";
                    cpp << "}\n\n";
                }
            }
        } else {
            // niechroniona funkcja: przepisywana bez zmian, tak jak w zrodle
            cpp << src.substr(f.sourceStart, f.sourceEnd - f.sourceStart) << "\n\n";
        }
    }

    size_t lastEnd = 0;
    // petla znajduje koniec ostatniej sparsowanej funkcji w oryginalnym zrodle
    for (auto& f : prog.functions) if (f.sourceEnd > lastEnd) lastEnd = f.sourceEnd;
    if (lastEnd < src.size()) cpp << src.substr(lastEnd);

    writeFile(outdir + "/" + base + ".protected.cpp", cpp.str());

    std::ostringstream meta;
    meta << "{\n";
    meta << "  \"seed\": " << seed << ",\n";
    meta << "  \"constKey\": " << enc.constKey << ",\n";
    meta << "  \"blockKey\": " << enc.blockKey << ",\n";
    meta << "  \"functions\": [\n";
    // petla zapisuje liste funkcji z metadanych (nazwa + czy chroniona) do JSON-a
    for (size_t i = 0; i < prog.functions.size(); ++i) {
        meta << "    { \"name\": \"" << prog.functions[i].name << "\", \"protected\": " << (prog.functions[i].protect ? "true" : "false") << " }";
        if (i + 1 < prog.functions.size()) meta << ",";
        meta << "\n";
    }
    meta << "  ]\n";
    meta << "}\n";
    writeFile(outdir + "/" + base + ".meta", meta.str());

    std::cout << "Wygenerowano: " << base << ".protected.cpp, .scarlett, .meta w " << outdir << "\n";
    return 0;
}
