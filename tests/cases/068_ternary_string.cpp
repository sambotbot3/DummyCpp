#include <iostream>
#include <string>

int classify(int n) {
    return n > 0 ? 1 : (n < 0 ? -1 : 0);
}

int main() {
    int x = 5;
    std::cout << (x > 3 ? "big" : "small") << std::endl;
    std::cout << (x == 5 ? "five" : "other") << std::endl;
    std::cout << classify(10) << std::endl;
    std::cout << classify(-3) << std::endl;
    std::cout << classify(0) << std::endl;
    std::string msg = (x > 0) ? "positive" : "non-positive";
    std::cout << msg << std::endl;
    return 0;
}
