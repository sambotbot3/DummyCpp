#include <iostream>
#include <string>

int main() {
    std::string word = "hello";
    int vowels = 0;
    for (char c : word) {
        if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u') vowels++;
    }
    std::cout << vowels << std::endl;
    std::string s = "hel";
    s.push_back('l');
    s.push_back('o');
    std::cout << s << std::endl;
    return 0;
}
