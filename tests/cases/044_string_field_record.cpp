#include <iostream>
#include <string>

class Person {
public:
  std::string name;
  int age;
  Person(const std::string &n, int a) : name(n), age(a) {}
  void print() const { std::cout << name.c_str() << "=" << age << std::endl; }
  int name_len() const { return name.size(); }
};

int main() {
  Person p("alice", 30);
  p.print();
  std::cout << p.name_len() << std::endl;
  return 0;
}
