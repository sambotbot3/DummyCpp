#include <iostream>

class Entity {
public:
  int id;
  Entity(int value) : id(value) {}
  int get_id() const { return id; }
};

class Player : public Entity {
public:
  int score;
  Player(int value, int start) : Entity(value), score(start) {}
  void add_score(int amount) { score += amount; }
  int total() const { return id + score; }
};

int inspect(Player p) {
  return p.get_id() + p.total() + p.id;
}

int main() {
  Player player(7, 20);
  player.add_score(5);
  int value = inspect(player);
  std::cout << "id=" << player.get_id() << std::endl;
  std::cout << "value=" << value << std::endl;
  return value;
}
