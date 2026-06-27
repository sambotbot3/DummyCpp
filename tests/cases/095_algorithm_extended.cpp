#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>

int main() {
    std::vector<int> v = {3, 1, 4, 1, 5, 9, 2, 6};

    std::cout << *std::min_element(v.begin(), v.end()) << std::endl;
    std::cout << *std::max_element(v.begin(), v.end()) << std::endl;
    std::cout << std::count(v.begin(), v.end(), 1) << std::endl;
    std::cout << std::accumulate(v.begin(), v.end(), 0) << std::endl;

    std::vector<double> d = {1.5, 2.5, 0.5, 3.0};
    std::cout << *std::min_element(d.begin(), d.end()) << std::endl;
    std::cout << *std::max_element(d.begin(), d.end()) << std::endl;

    return 0;
}
