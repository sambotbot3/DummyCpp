#include <iostream>
#include <string>

struct Address {
    std::string city;
    int zip;
    Address(const std::string &c, int z) : city(c), zip(z) {}
};

struct Person {
    std::string name;
    Address addr;
    Person(const std::string &n, const std::string &c, int z) : name(n), addr(c, z) {}
    void print() const {
        std::cout << name.c_str() << " lives in " << addr.city.c_str() << " " << addr.zip << std::endl;
    }
};

int main() {
    Person p("Alice", "NYC", 10001);
    p.print();
    Person q("Bob", "LA", 90001);
    q.print();
    return 0;
}
