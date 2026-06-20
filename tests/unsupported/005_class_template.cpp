template <typename T>
struct Box {
  T value;
};

int main() {
  Box<int> box{3};
  return box.value;
}
