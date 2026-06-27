#include <algorithm>
#include <iostream>
#include <numeric>
#include <vector>

int main() {
    std::vector<int> v = {1, 2, 3, 4, 5};

    if (std::find(v.begin(), v.end(), 3) != v.end()) {
        std::cout << "found" << std::endl;
    }
    if (std::find(v.begin(), v.end(), 9) == v.end()) {
        std::cout << "not found" << std::endl;
    }

    std::cout << std::count(v.begin(), v.end(), 3) << std::endl;

    int sum = std::accumulate(v.begin(), v.end(), 0);
    std::cout << sum << std::endl;

    return 0;
}
