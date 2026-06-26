#include <iostream>

class Timer {
    int elapsed = 0;
    bool running = false;
public:
    Timer() {}
    void tick() { if (running) ++elapsed; }
    void start() { running = true; }
    int get() const { return elapsed; }
    bool active() const { return running; }
};

int main() {
    Timer t;
    std::cout << t.get() << std::endl;
    std::cout << t.active() << std::endl;
    t.start();
    t.tick();
    t.tick();
    std::cout << t.get() << std::endl;
    return 0;
}
