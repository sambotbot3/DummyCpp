#include <iostream>

class Widget {
    static int count;
public:
    Widget() { ++count; }
    static int getCount() { return count; }
};

int Widget::count = 0;

int main() {
    std::cout << Widget::getCount() << std::endl;
    Widget a;
    Widget b;
    std::cout << Widget::getCount() << std::endl;
    return 0;
}
