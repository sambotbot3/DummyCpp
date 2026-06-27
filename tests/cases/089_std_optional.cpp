#include <iostream>
#include <optional>

std::optional<int> find_positive(int x) {
    if (x > 0) return x;
    return std::nullopt;
}

std::optional<int> get_val() { return 42; }

int main() {
    std::optional<int> a = find_positive(5);
    std::optional<int> b = find_positive(-3);
    std::optional<int> c = get_val();

    if (a.has_value()) std::cout << a.value() << std::endl;
    if (!b.has_value()) std::cout << "none" << std::endl;
    if (c.has_value()) std::cout << c.value() << std::endl;
    return 0;
}
