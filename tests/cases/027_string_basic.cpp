#include <iostream>
#include <string>

int main() {
  std::string label = "alpha";
  std::string copy(label);
  label = "beta";
  std::cout << "label=" << label.c_str() << std::endl;
  std::cout << "copy=" << copy.c_str() << " size=" << copy.size() << std::endl;
  return label.size() + copy.size();
}
