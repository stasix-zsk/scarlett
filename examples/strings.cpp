// strings.cpp: przykladowy plik wejsciowy dla scarlett - pokazuje obsluge
// typu string w chronionych funkcjach: konkatenacja, porownanie oraz
// budowanie napisu w petli. Uzyte operacje sa poprawnym C++ (std::string),
// wiec plik kompiluje sie zarowno natywnie, jak i po obfuskacji - wynik jest
// identyczny. (Wbudowane len(s) celowo pominieto, bo nie jest funkcja C++.)
#include <iostream>
#include <string>

[[vm_protect]] std::string join(std::string a, std::string b) {
    return a + "/" + b;
}

[[vm_protect]] int same(std::string a, std::string b) {
    if (a == b) return 1;
    return 0;
}

[[vm_protect]] std::string repeat(std::string s, int n) {
    std::string r = "";
    // petla sklada napis z n kopii s
    for (int i = 0; i < n; i = i + 1) {
        r = r + s;
    }
    return r;
}

int main() {
    std::cout << join("foo", "bar") << "\n";
    std::cout << same("abc", "abc") << " " << same("abc", "xyz") << "\n";
    std::cout << repeat("ab", 3) << "\n";
    return 0;
}
