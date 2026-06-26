#include <iostream>
#include <map>

int main() {
    std::map<int, int> scores;
    scores[1] = 10;
    scores[2] = 20;
    std::cout << scores.count(1) << std::endl;
    std::cout << scores.count(3) << std::endl;
    return 0;
}
