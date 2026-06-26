#include <iostream>

enum class Direction { North, South, East, West };

int main() {
    Direction d = Direction::North;
    if (d == Direction::North) {
        std::cout << "north" << std::endl;
    }
    Direction e = Direction::West;
    std::cout << (int)e << std::endl;
    return 0;
}
