#include <iostream>
#include <vector>

class Stack {
    std::vector<int> data;
public:
    void push(int v) { data.push_back(v); }
    int pop() {
        int v = data.back();
        data.pop_back();
        return v;
    }
    bool empty() const { return data.empty(); }
    int size() const { return data.size(); }
};

int main() {
    Stack s;
    s.push(10);
    s.push(20);
    s.push(30);
    std::cout << s.size() << std::endl;
    std::cout << s.pop() << std::endl;
    std::cout << s.size() << std::endl;
    std::cout << s.empty() << std::endl;
    return 0;
}
