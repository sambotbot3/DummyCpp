#include <iostream>
#include <map>
#include <string>

struct Scores {
    std::map<std::string, int> data;
    void add(const std::string &name, int score) {
        data[name] = score;
    }
    int count() {
        return data.size();
    }
};

int main() {
    Scores s;
    s.add("alice", 90);
    s.add("bob", 75);
    std::cout << s.count() << std::endl;
    return 0;
}
