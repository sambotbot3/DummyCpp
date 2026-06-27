#include <iostream>
#include <map>

int main() {
    std::map<int, int> scores;
    scores[1] = 10;
    scores[2] = 20;
    scores[3] = 30;

    if (scores.find(2) != scores.end()) {
        std::cout << "found 2" << std::endl;
    }
    if (scores.find(9) == scores.end()) {
        std::cout << "not found 9" << std::endl;
    }

    scores.erase(2);
    std::cout << scores.count(2) << std::endl;
    std::cout << scores.count(1) << std::endl;

    return 0;
}
