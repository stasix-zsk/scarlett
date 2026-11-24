// demo.cpp: przykladowy plik wejsciowy dla scarlett.exe - add() zostaje
// niechroniona, mul() i factorial() sa oznaczone [[vm_protect]] i trafiaja
// do obfuskacji. Brak petli w tym pliku (factorial jest rekurencyjny).
#include <iostream>

int add(int a, int b) {
    return a + b;
}

[[vm_protect]] int mul(int a, int b) {
    return a * b;
}

[[vm_protect]] int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

int main() {
    std::cout << add(2,3) << "\n";
    std::cout << mul(4,5) << "\n";
    std::cout << factorial(6) << "\n";
    return 0;
}
