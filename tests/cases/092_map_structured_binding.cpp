#include <iostream>
#include <map>

int main() {
    std::map<std::string, int> scores;
    scores["alice"] = 95;
    scores["bob"] = 87;
    scores["carol"] = 92;

    int total = 0;
    for (auto& [name, score] : scores) {
        std::cout << name << ":" << score << std::endl;
        total += score;
    }
    std::cout << total << std::endl;
    return 0;
}
