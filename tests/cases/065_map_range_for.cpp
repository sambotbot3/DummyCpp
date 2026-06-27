#include <iostream>
#include <map>
#include <string>

int main() {
    std::map<std::string, int> scores;
    scores["alice"] = 10;
    scores["bob"] = 20;
    scores["charlie"] = 30;
    int total = 0;
    for (const auto &p : scores) {
        std::cout << p.first << "=" << p.second << std::endl;
        total += p.second;
    }
    std::cout << total << std::endl;
    return 0;
}
