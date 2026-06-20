#include <iostream>
#include <string>

int main() {
  std::string label = "alpha";
  label += "-";
  std::string suffix = "beta";
  label.append(suffix);
  const int before_clear = label.empty() ? 0 : label.size();
  label.clear();
  label += "z";
  std::cout << "before=" << before_clear << std::endl;
  std::cout << "label=" << label.c_str() << " empty=" << label.empty() << std::endl;
  return before_clear + label.size();
}
