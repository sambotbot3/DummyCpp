#include <iostream>
#include <vector>

struct Point {
    int x;
    int y;
    Point(int x, int y) : x(x), y(y) {}
    void print() const {
        std::cout << "(" << x << "," << y << ")" << std::endl;
    }
};

int main() {
    std::vector<Point> pts;
    pts.push_back(Point(1, 2));
    pts.push_back(Point(3, 4));
    pts.push_back(Point(5, 6));
    for (size_t i = 0; i < pts.size(); ++i) {
        pts[i].print();
    }
    pts.erase(pts.begin() + 1);
    std::cout << pts.size() << std::endl;
    return 0;
}
