#include <contara/contara.hpp>

#include <iostream>

template<class R>
void print(const char* label, const R& range) {
    std::cout << label << ": ";
    for (const auto& value : range) std::cout << value << ' ';
    std::cout << '\n';
}

int main() {
    using namespace contara::ops;
    const std::set<int> a{1, 2, 3}, b{3, 4, 5};
    print("Union", a | b);
    print("Intersection", a & b);
    print("Difference", a - b);
    print("Symmetric difference", a ^ b);

    const std::vector<int> x{3, 1, 3}, y{2, 4};
    print("Concatenation", x + y);
    print("Sorted unique", contara::to_set(x + y));
    const auto squared = contara::parallel_map(x + y, [](int value) { return value * value; },
        {.workers = 2, .min_per_worker = 1});
    print("Parallel squares (original order)", squared);

    std::cout << "Zip pairs: ";
    for (auto&& [left, right] : contara::zip(x, y)) std::cout << '(' << left << ", " << right << ") ";
    std::cout << '\n';
    auto sums = contara::zip(x, y) | std::views::transform([](auto pair) { return pair.first + pair.second; });
    print("Zipped sums", contara::to_vector(sums));
}
