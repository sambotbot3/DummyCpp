#include <iostream>
#include <vector>

int main() {
    std::vector<int> v = {1, 2, 3};
    int sum = 0;
    for (const auto &x : v) {
        sum += x;
    }
    for (auto x : v) {
        sum += x;
    }
    std::cout << sum << std::endl;
    return 0;
}
