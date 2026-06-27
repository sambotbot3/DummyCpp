#include <iostream>

struct Widget {
    int id;
    Widget(int id) : id(id) { std::cout << "create " << id << std::endl; }
    ~Widget() { std::cout << "destroy " << id << std::endl; }
};

int main() {
    Widget a(1);
    {
        Widget b(2);
        std::cout << "inner" << std::endl;
    }
    std::cout << "outer" << std::endl;
    return 0;
}
