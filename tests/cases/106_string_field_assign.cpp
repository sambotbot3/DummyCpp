#include <iostream>
#include <string>

class Logger {
public:
    std::string prefix;
    Logger(const std::string &p) : prefix(p) {}
    void log(const std::string &msg) const {
        std::cout << prefix.c_str() << ": " << msg.c_str() << std::endl;
    }
    void set_prefix(const std::string &p) {
        prefix = p;
    }
};

int main() {
    Logger l("INFO");
    l.log("hello");
    l.set_prefix("DEBUG");
    l.log("world");
    return 0;
}
