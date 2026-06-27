#include <iostream>
#include <string>
#include <vector>

class Library {
    std::vector<std::string> books;
public:
    void add(const std::string &book) { books.push_back(book); }
    int size() const { return books.size(); }
    void list() const {
        for (size_t i = 0; i < books.size(); ++i) {
            std::cout << i + 1 << ". " << books[i] << std::endl;
        }
    }
};

int main() {
    Library lib;
    lib.add("C++ Primer");
    lib.add("SICP");
    lib.add("CLRS");
    lib.list();
    std::cout << lib.size() << std::endl;
    return 0;
}
