#include <iostream>
#include <string>

class Builder {
public:
    std::string result;
    Builder() {}
    void add(const std::string &s) {
        result += s;
    }
};

int main() {
    Builder b;
    b.add("hello");
    b.add(" world");
    std::cout << b.result.c_str() << std::endl;
    std::cout << b.result.size() << std::endl;
    return 0;
}
