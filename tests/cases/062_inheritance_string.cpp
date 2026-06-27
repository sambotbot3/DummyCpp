#include <iostream>
#include <string>

class Animal {
public:
    std::string name;
    int age;
    Animal(const std::string &n, int a) : name(n), age(a) {}
    void speak() const { std::cout << name << " says hello" << std::endl; }
};

class Dog : public Animal {
public:
    std::string breed;
    Dog(const std::string &n, int a, const std::string &b)
        : Animal(n, a), breed(b) {}
    void info() const {
        std::cout << name << " is a " << breed << " age " << age << std::endl;
    }
};

int main() {
    Dog d("Rex", 3, "Labrador");
    d.speak();
    d.info();
    return 0;
}
