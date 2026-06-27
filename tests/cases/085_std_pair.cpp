#include <iostream>
#include <utility>

std::pair<int, int> minmax(int a, int b) {
    if (a < b) return {a, b};
    return {b, a};
}

std::pair<int, int> swap_pair(std::pair<int, int> p) {
    return {p.second, p.first};
}

int main() {
    auto p = minmax(5, 3);
    std::cout << p.first << " " << p.second << std::endl;
    std::pair<int, int> q = swap_pair(p);
    std::cout << q.first << " " << q.second << std::endl;
    return 0;
}
