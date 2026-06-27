#include <iostream>
#include <memory>

struct Point {
    int x, y;
    Point(int x, int y) : x(x), y(y) {}
};

int main() {
    auto p = std::make_unique<Point>(3, 4);
    std::cout << p->x << std::endl;
    std::cout << p->y << std::endl;

    std::unique_ptr<Point> q(new Point(10, 20));
    std::cout << q->x << std::endl;

    std::shared_ptr<Point> a = std::make_shared<Point>(5, 6);
    std::cout << a->x << std::endl;
    std::shared_ptr<Point> b = a;
    std::cout << b->y << std::endl;

    return 0;
}
