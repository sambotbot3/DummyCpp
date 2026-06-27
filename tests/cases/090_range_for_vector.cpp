#include <iostream>
#include <vector>

int main() {
    std::vector<int> v = {1, 2, 3, 4, 5};

    // single-statement form
    int sum = 0;
    for (auto x : v) sum += x;
    std::cout << sum << std::endl;

    // block form
    int product = 1;
    for (auto x : v) {
        product *= x;
    }
    std::cout << product << std::endl;

    // const ref form
    int count = 0;
    for (const int &x : v) {
        if (x % 2 == 0) count++;
    }
    std::cout << count << std::endl;

    return 0;
}
