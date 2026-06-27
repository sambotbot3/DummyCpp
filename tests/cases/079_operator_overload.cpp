#include <iostream>

struct Point {
    int x, y;
    Point(int x, int y) : x(x), y(y) {}
    bool operator==(const Point &o) const { return x == o.x && y == o.y; }
    bool operator!=(const Point &o) const { return !(*this == o); }
    bool operator<(const Point &o) const { return x < o.x || (x == o.x && y < o.y); }
};

int main() {
    Point a(1, 2), b(1, 2), c(3, 4);
    std::cout << (a == b) << std::endl;
    std::cout << (a == c) << std::endl;
    std::cout << (a != c) << std::endl;
    std::cout << (a < c) << std::endl;
    std::cout << (c < a) << std::endl;
    return 0;
}
