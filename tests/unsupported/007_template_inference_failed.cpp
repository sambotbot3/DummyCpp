struct Box {
  int value;
};

Box make_box() {
  return Box{3};
}

template <typename T>
T identity(T value) {
  return value;
}

int main() {
  return identity(make_box()).value;
}
