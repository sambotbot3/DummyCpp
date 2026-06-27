#include <iostream>

struct Logger {
    int id;
    Logger(int id) : id(id) { std::cout << "new " << id << std::endl; }
    ~Logger() { std::cout << "del " << id << std::endl; }
};

int main() {
    for (int i = 0; i < 3; ++i) {
        Logger l(i);
        if (i == 1) break;
        std::cout << i << std::endl;
    }
    std::cout << "done" << std::endl;
    return 0;
}
