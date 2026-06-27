#include <iostream>
#include <string>
#include <vector>

int main() {
    std::vector<std::string> words;
    words.push_back("hello");
    words.push_back("world");
    for (const std::string &w : words) {
        std::cout << w << std::endl;
    }
    std::cout << words.size() << std::endl;
    return 0;
}
