#include <iostream>
#include <string>
#include <vector>

class Roster {
    std::vector<std::string> names;
    std::string title;
public:
    Roster(const std::string &t) : title(t) {}
    void add(const std::string &name) { names.push_back(name); }
    int count() const { return names.size(); }
    void print() const {
        std::cout << title << ": " << count() << std::endl;
    }
};

int main() {
    Roster r("Class");
    r.add("alice");
    r.add("bob");
    r.print();
    return 0;
}
