#include <iostream>
#include <string>

class Animal {
public:
    std::string name;
    Animal(const std::string &n) : name(n) {}
    std::string get_name() const { return name; }
};

class Dog : public Animal {
public:
    std::string trick;
    Dog(const std::string &n, const std::string &t) : Animal(n), trick(t) {}
    void show() const {
        std::cout << get_name() << " knows " << trick << std::endl;
    }
};

int main() {
    Dog d("Rex", "fetch");
    d.show();
    std::cout << d.get_name() << std::endl;
    return 0;
}
