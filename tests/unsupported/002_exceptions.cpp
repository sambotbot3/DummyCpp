#include <iostream>

int main() {
  try {
    throw 1;
  } catch (int value) {
    return value;
  }
}
