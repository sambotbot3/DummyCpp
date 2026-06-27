#include <iostream>
#include <string>

class Person {
public:
    std::string name;
    int age;
    Person(const std::string &n, int a) : name(n), age(a) {}
    void print() const { std::cout << name.c_str() << "=" << age << std::endl; }
};

int main() {
    Person p("alice", 30);
    p.print();
    Person q("bob", 25);
    q.print();
    return 0;
}
