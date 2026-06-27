#include <iostream>

struct Point {
    int x, y;
    Point(int x, int y) : x(x), y(y) {}
    int sum() const { return x + y; }
    int dist2() const { return x * x + y * y; }
};

int main() {
    Point p(3, 4);
    std::cout << p.sum() << std::endl;
    std::cout << p.dist2() << std::endl;
    return 0;
}
