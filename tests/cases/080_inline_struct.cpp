#include <iostream>

struct Point { int x, y; Point(int x, int y) : x(x), y(y) {} int sum() const { return x + y; } };

void show(const Point &p) { std::cout << p.x << "," << p.y << std::endl; }

int main() {
    Point a(3, 4), b(1, 2);
    show(a);
    show(b);
    std::cout << a.sum() << std::endl;
    std::cout << b.sum() << std::endl;
    return 0;
}
