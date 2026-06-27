#include <iostream>

struct Point {
    int x, y;
    Point() : x(0), y(0) {}
    Point(int x, int y) : x(x), y(y) {}
    void print() const { std::cout << x << "," << y << std::endl; }
};

int main() {
    Point a;
    Point b(3, 4);
    a.print();
    b.print();
    return 0;
}
