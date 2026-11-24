// Codec.cpp: szyfrowanie/deszyfrowanie modulu VM do/z formatu binarnego
// .scarlett - tasowanie opkodow, szyfrowanie XOR bloku kodu i suma
// kontrolna FNV-1a do wykrywania manipulacji danymi.
#include "scarlett/Codec.h"
#include <algorithm>
#include <cstring>
#include <random>

namespace scarlett {

// --- zapis/odczyt liczb w formacie little-endian ---

static void putU8(std::vector<uint8_t>& b, uint8_t v){ b.push_back(v); }
static void putU16(std::vector<uint8_t>& b, uint16_t v){ b.push_back(v&0xFF); b.push_back((v>>8)&0xFF); }
static void putU32(std::vector<uint8_t>& b, uint32_t v){
    // petla zapisuje 4 bajty liczby, od najmlodszego
    for(int i=0;i<4;i++) b.push_back((v>>(i*8))&0xFF);
}
static void putU64(std::vector<uint8_t>& b, uint64_t v){
    // petla zapisuje 8 bajtow liczby, od najmlodszego
    for(int i=0;i<8;i++) b.push_back((v>>(i*8))&0xFF);
}
static void putI32(std::vector<uint8_t>& b, int32_t v){ putU32(b, (uint32_t)v); }

static uint8_t getU8(const std::vector<uint8_t>& b, size_t& i){ if(i>=b.size()) return 0; return b[i++]; }
static uint16_t getU16(const std::vector<uint8_t>& b, size_t& i){ uint16_t v = b[i] | (b[i+1]<<8); i+=2; return v; }
static uint32_t getU32(const std::vector<uint8_t>& b, size_t& i){
    uint32_t v=0;
    // petla odczytuje 4 bajty i skleja je w jedna liczbe
    for(int k=0;k<4;k++) v |= ((uint32_t)b[i+k])<<(k*8);
    i+=4; return v;
}
static uint64_t getU64(const std::vector<uint8_t>& b, size_t& i){
    uint64_t v=0;
    // petla odczytuje 8 bajtow i skleja je w jedna liczbe
    for(int k=0;k<8;k++) v |= ((uint64_t)b[i+k])<<(k*8);
    i+=8; return v;
}
static int32_t getI32(const std::vector<uint8_t>& b, size_t& i){ return (int32_t)getU32(b,i); }

// Suma kontrolna FNV-1a - integralnosc zdekodowanego kodu wszystkich funkcji.
static uint32_t fnv1a(const std::vector<uint8_t>& data) {
    uint32_t h = 0x811c9dc5u;
    // petla miesza kazdy bajt danych w akumulator sumy
    for (uint8_t c : data) { h ^= c; h *= 0x01000193u; }
    return h;
}

EncodeResult encodeModule(const VMModule& mod, uint64_t seed) {
    EncodeResult res;
    res.seed = seed;
    std::mt19937_64 rng(seed);
    res.constKey = (uint32_t)rng();
    res.blockKey = (uint32_t)rng();

    res.opcodeMap.resize(VM_OP_COUNT);
    // petla wypelnia mape tozsamosciowa (0,1,2,...) przed przetasowaniem
    for (int i=0;i<VM_OP_COUNT;i++) res.opcodeMap[i]=i;
    std::shuffle(res.opcodeMap.begin(), res.opcodeMap.end(), rng);

    std::vector<std::vector<uint8_t>> funcBytes(mod.functions.size());
    uint32_t checksumInput = 0;
    std::vector<uint8_t> allDecoded;
    // petla serializuje instrukcje kazdej funkcji do bajtow (bez szyfrowania blokowego)
    for (size_t fi=0; fi<mod.functions.size(); ++fi) {
        const VMFunction& f = mod.functions[fi];
        std::vector<uint8_t> fb;
        // petla koduje kazda instrukcje funkcji wg jej opkodu
        for (const VMInst& ins : f.code) {
            fb.push_back(res.opcodeMap[(int)ins.op]);
            switch (ins.op) {
                case VMOp::NOP: break;
                case VMOp::LOADI:
                    fb.push_back((uint8_t)ins.dst);
                    putI32(fb, (int32_t)(ins.imm ^ res.constKey));
                    break;
                case VMOp::MOV:
                    fb.push_back((uint8_t)ins.dst); fb.push_back((uint8_t)ins.a); break;
                case VMOp::ADD: case VMOp::SUB: case VMOp::MUL: case VMOp::DIV: case VMOp::MOD:
                case VMOp::CMPEQ: case VMOp::CMPNE: case VMOp::CMPLT: case VMOp::CMPLE: case VMOp::CMPGT: case VMOp::CMPGE:
                case VMOp::AND: case VMOp::OR: case VMOp::XOR:
                    fb.push_back((uint8_t)ins.dst); fb.push_back((uint8_t)ins.a); fb.push_back((uint8_t)ins.b); break;
                case VMOp::NEG: case VMOp::NOT:
                    fb.push_back((uint8_t)ins.dst); fb.push_back((uint8_t)ins.a); break;
                case VMOp::JMP:
                    putI32(fb, ins.target ^ (int32_t)res.constKey); break;
                case VMOp::JMPIF:
                    fb.push_back((uint8_t)ins.a);
                    putI32(fb, ins.target ^ (int32_t)res.constKey); break;
                case VMOp::CALL:
                    fb.push_back((uint8_t)ins.dst);
                    putU16(fb, (uint16_t)ins.funcIndex);
                    fb.push_back((uint8_t)ins.args.size());
                    // petla zapisuje rejestry argumentow wywolania
                    for (int r : ins.args) fb.push_back((uint8_t)r);
                    break;
                case VMOp::RET:
                    fb.push_back((uint8_t)ins.a); break;
                case VMOp::RETV:
                    break;
                default: break;
            }
        }
        funcBytes[fi] = fb;
        allDecoded.insert(allDecoded.end(), fb.begin(), fb.end());
    }
    uint32_t checksum = fnv1a(allDecoded);

    std::vector<uint8_t> out;

    // Naglowek pliku: magic "SCVM", wersja, seed, klucze, mapa opkodow.
    out.push_back('S'); out.push_back('C'); out.push_back('V'); out.push_back('M');
    putU32(out, 1);
    putU64(out, seed);
    putU32(out, res.constKey);
    putU32(out, res.blockKey);
    // petla zapisuje przetasowana mape opkodow bajt po bajcie
    for (int i=0;i<VM_OP_COUNT;i++) out.push_back(res.opcodeMap[i]);
    putU32(out, (uint32_t)mod.functions.size());

    // petla zapisuje metadane i zaszyfrowany kod kazdej funkcji
    for (size_t fi=0; fi<mod.functions.size(); ++fi) {
        const VMFunction& f = mod.functions[fi];

        putU16(out, (uint16_t)f.name.size());
        // petla zapisuje nazwe funkcji znak po znaku
        for (char c : f.name) out.push_back((uint8_t)c);
        out.push_back((uint8_t)f.retType);
        out.push_back((uint8_t)f.paramCount);
        putU16(out, (uint16_t)f.regCount);

        std::vector<uint8_t> enc = funcBytes[fi];
        // petla szyfruje kazdy bajt kodu funkcji XOR-em z blockKey i pozycja bajtu
        for (size_t i=0;i<enc.size();++i) {
            uint8_t k = (uint8_t)(res.blockKey >> ((i % 4) * 8));
            enc[i] ^= k;
            enc[i] ^= (uint8_t)(i & 0xFF);
        }
        putU32(out, (uint32_t)enc.size());
        // petla dopisuje zaszyfrowane bajty funkcji do wyjscia
        for (uint8_t c : enc) out.push_back(c);
    }
    putU32(out, checksum);
    res.bytes = out;
    return res;
}

VMModule decodeModule(const std::vector<uint8_t>& bytes, std::string& error, bool* integrityOk) {
    VMModule mod;
    if (integrityOk) *integrityOk = false;
    size_t i = 0;
    if (bytes.size() < 4 || bytes[0]!='S'||bytes[1]!='C'||bytes[2]!='V'||bytes[3]!='M') {
        error = "zly magic"; return mod;
    }
    i = 4;
    uint32_t version = getU32(bytes, i);
    if (version != 1) {         error = "zla wersja"; return mod; }
    uint64_t seed = getU64(bytes, i);
    uint32_t constKey = getU32(bytes, i);
    uint32_t blockKey = getU32(bytes, i);
    std::vector<uint8_t> opcodeMap(VM_OP_COUNT);
    // petla odczytuje mape opkodow zapisana w naglowku
    for (int k=0;k<VM_OP_COUNT;k++) opcodeMap[k] = getU8(bytes, i);

    std::vector<int> inverse(256, -1);
    // petla buduje odwrotna permutacje: wartosc fizyczna -> wartosc logiczna opkodu
    for (int l=0;l<VM_OP_COUNT;l++) {
        inverse[opcodeMap[l]] = l;
    }
    uint32_t funcCount = getU32(bytes, i);

    std::vector<uint8_t> allDecoded;
    mod.seed = seed;
    // petla odtwarza kazda funkcje modulu z zaszyfrowanych bajtow
    for (uint32_t f=0; f<funcCount; ++f) {
        uint16_t nameLen = getU16(bytes, i);
        std::string name;
        // petla odczytuje nazwe funkcji znak po znaku
        for (int k=0;k<nameLen;k++) name.push_back((char)getU8(bytes,i));
        uint8_t retType = getU8(bytes,i);
        uint8_t paramCount = getU8(bytes,i);
        uint16_t regCount = getU16(bytes,i);
        uint32_t codeSize = getU32(bytes,i);

        std::vector<uint8_t> enc(bytes.begin()+i, bytes.begin()+i+codeSize);
        i += codeSize;

        std::vector<uint8_t> dec = enc;
        // petla odszyfrowuje blok kodu (odwrotnosc szyfrowania z encodeModule)
        for (size_t k=0;k<dec.size();++k) {
            uint8_t key = (uint8_t)(blockKey >> ((k % 4) * 8));
            dec[k] ^= key;
            dec[k] ^= (uint8_t)(k & 0xFF);
        }
        allDecoded.insert(allDecoded.end(), dec.begin(), dec.end());

        VMFunction vf;
        vf.name = name;
        vf.retType = (ValueType)retType;
        vf.paramCount = paramCount;
        vf.regCount = regCount;
        size_t j = 0;
        // petla odtwarza instrukcje funkcji, dekodujac opkod po opkodzie
        while (j < dec.size()) {
            uint8_t phys = dec[j++];
            int log = (phys < inverse.size()) ? inverse[phys] : -1;
            if (log < 0 || log >= VM_OP_COUNT) { error = "zly opcode"; return mod; }
            VMOp op = (VMOp)log;
            VMInst ins; ins.op = op;
            switch (op) {
                case VMOp::NOP: break;
                case VMOp::LOADI:
                    ins.dst = dec[j++];
                    ins.imm = (int32_t)getU32(dec, j) ^ (int32_t)constKey;
                    break;
                case VMOp::MOV:
                    ins.dst = dec[j++]; ins.a = dec[j++]; break;
                case VMOp::ADD: case VMOp::SUB: case VMOp::MUL: case VMOp::DIV: case VMOp::MOD:
                case VMOp::CMPEQ: case VMOp::CMPNE: case VMOp::CMPLT: case VMOp::CMPLE: case VMOp::CMPGT: case VMOp::CMPGE:
                case VMOp::AND: case VMOp::OR: case VMOp::XOR:
                    ins.dst = dec[j++]; ins.a = dec[j++]; ins.b = dec[j++]; break;
                case VMOp::NEG: case VMOp::NOT:
                    ins.dst = dec[j++]; ins.a = dec[j++]; break;
                case VMOp::JMP:
                    ins.target = (int32_t)getU32(dec, j) ^ (int32_t)constKey; break;
                case VMOp::JMPIF:
                    ins.a = dec[j++];
                    ins.target = (int32_t)getU32(dec, j) ^ (int32_t)constKey; break;
                case VMOp::CALL:
                    ins.dst = dec[j++];
                    ins.funcIndex = getU16(dec, j);
                    ins.func = "";
                    {
                        uint8_t argc = dec[j++];
                        // petla odczytuje rejestry argumentow wywolania
                        for (int k=0;k<argc;k++) ins.args.push_back(dec[j++]);
                    }
                    break;
                case VMOp::RET:
                    ins.a = dec[j++]; break;
                case VMOp::RETV:
                    break;
                default: break;
            }
            vf.code.push_back(ins);
        }
        mod.functions.push_back(vf);
    }

    uint32_t stored = getU32(bytes, i);
    uint32_t computed = fnv1a(allDecoded);
    if (stored != computed) {
        if (integrityOk) *integrityOk = false;
    } else {
        if (integrityOk) *integrityOk = true;
    }

    return mod;
}
}
