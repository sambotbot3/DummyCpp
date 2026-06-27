#include <iostream>
#include <vector>

int main() {
    std::vector<int> v = {1, 2, 3, 4, 5};
    v.erase(v.begin() + 2);
    for (size_t i = 0; i < v.size(); ++i) {
        std::cout << v[i] << std::endl;
    }
    v.insert(v.begin() + 1, 99);
    std::cout << v[1] << std::endl;
    v.erase(v.begin());
    std::cout << v[0] << std::endl;
    return 0;
}
