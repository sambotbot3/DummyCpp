#include <iostream>
#include <string>

int count_vowels(const std::string &s) {
    int count = 0;
    for (int i = 0; i < (int)s.size(); i++) {
        char c = s[i];
        if (c=='a'||c=='e'||c=='i'||c=='o'||c=='u') count++;
    }
    return count;
}

void print_upper(const std::string &s) {
    for (int i = 0; i < (int)s.size(); i++) {
        char c = s[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        std::cout << c;
    }
    std::cout << std::endl;
}

int main() {
    std::string word = "hello";
    std::cout << count_vowels(word) << std::endl;
    print_upper(word);
    return 0;
}
