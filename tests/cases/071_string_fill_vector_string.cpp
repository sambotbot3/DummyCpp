#include <iostream>
#include <string>
#include <vector>

int main() {
    std::string sep(3, '-');
    std::cout << sep << std::endl;

    std::vector<std::string> words;
    words.push_back("apple");
    words.push_back("banana");
    words.push_back("cherry");
    std::cout << words.size() << std::endl;
    for (const std::string &w : words) {
        std::cout << w << std::endl;
    }
    return 0;
}
