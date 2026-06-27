#include <iostream>
#include <map>
int main() {
    std::map<int, int> scores;
    scores[1] = 10;
    scores[2] = 20;
    std::cout << scores.count(1) << std::endl;
    std::cout << scores.count(3) << std::endl;
    if (scores.find(2) != scores.end()) {
        std::cout << "found2" << std::endl;
    }
    if (scores.find(5) == scores.end()) {
        std::cout << "notfound5" << std::endl;
    }
    return 0;
}
