#include <iostream>
#include <vector>
#include <algorithm>

int main() {
    std::vector<int> v = {3, 1, 4, 1, 5, 9, 2, 6};
    std::sort(v.begin(), v.end(), [](int a, int b) { return a < b; });
    for (const int &x : v) std::cout << x << " ";
    std::cout << std::endl;
    return 0;
}
