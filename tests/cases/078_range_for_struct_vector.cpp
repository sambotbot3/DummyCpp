#include <iostream>
#include <vector>

struct Point {
    int x, y;
    Point(int x, int y) : x(x), y(y) {}
    void print() const { std::cout << x << "," << y << std::endl; }
    int sum() const { return x + y; }
};

int main() {
    std::vector<Point> pts;
    pts.push_back(Point(1, 2));
    pts.push_back(Point(3, 4));
    pts.push_back(Point(5, 6));
    for (const Point &p : pts) {
        p.print();
        std::cout << p.sum() << std::endl;
    }
    return 0;
}
