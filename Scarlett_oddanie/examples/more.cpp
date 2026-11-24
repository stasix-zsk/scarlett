// more.cpp: drugi przykladowy plik wejsciowy dla scarlett.exe - fib, sum
// i max2 sa oznaczone [[vm_protect]] i pokazuja rekurencje, petle while
// oraz if/else w chronionym kodzie.
#include <iostream>

[[vm_protect]] int fib(int n) {
    if (n <= 1) return n;
    return fib(n-1) + fib(n-2);
}

[[vm_protect]] int sum(int n) {
    int s = 0;
    int i = 0;
    // petla sumuje liczby od 0 do n
    while (i <= n) {
        s = s + i;
        i = i + 1;
    }
    return s;
}

[[vm_protect]] int max2(int a, int b) {
    if (a > b) return a;
    return b;
}

int main() {
    std::cout << fib(10) << "\n";
    std::cout << sum(10) << "\n";
    std::cout << max2(7,3) << "\n";
    return 0;
}
