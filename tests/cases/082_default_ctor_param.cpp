#include <iostream>

struct Counter {
    int value;
    Counter(int start = 0) : value(start) {}
    void inc() { ++value; }
    int get() const { return value; }
};

int main() {
    Counter a;
    Counter b(10);
    a.inc(); a.inc();
    b.inc();
    std::cout << a.get() << std::endl;
    std::cout << b.get() << std::endl;
    return 0;
}
