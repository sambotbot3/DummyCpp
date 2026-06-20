struct Box {
  int value;
};

template <typename T>
T identity(T value) {
  return value;
}

int main() {
  Box box{3};
  return identity(box).value;
}
