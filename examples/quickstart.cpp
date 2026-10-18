#include <contara/contara.hpp>

#include <array>
#include <iostream>

template<class R>
void print(const char* label, const R& range) {
    std::cout << label << ": ";
    for (const auto& value : range) std::cout << value << ' ';
    std::cout << '\n';
}

int main() {
    using namespace contara::ops;
    using contara::zip;
    const std::vector<int> x{3, 1, 3}, y{2, 4};
    auto joined = x + y;
    print("Vector +", joined);
    std::vector<int> assigned;
    assigned = x + y;
    print("Vector assignment", assigned);
    auto appended = x;
    appended += y;
    print("Vector +=", appended);

    std::deque<int> queue{1, 2}, extra{3, 4};
    print("Deque +", queue + extra);
    queue += extra;
    print("Deque +=", queue);
    std::list<int> first{5, 6}, second{7, 8};
    print("List +", first + second);
    first += second;
    print("List +=", first);

    const std::set<int> a{1, 2, 3}, b{3, 4, 5};
    print("Union", a | b);
    print("Intersection", a & b);
    print("Difference", a - b);
    print("Symmetric difference", a ^ b);

    const std::unordered_set<int> hashed_a{1, 2, 3}, hashed_b{2, 4};
    print("Unordered intersection", hashed_a & hashed_b);
    print("Sorted unique", contara::to_set(joined));
    const std::array values{3, 1, 3, 2, 4};
    print("Array to vector", contara::to_vector(values));
    print("Array to deque", contara::to_deque(values));
    print("Array to list", contara::to_list(values));
    const auto squared = contara::parallel_map(joined, [](int value) { return value * value; },
        {.workers = 2, .min_per_worker = 1});
    print("Parallel squares (original order)", squared);

    std::cout << "Zip pairs: ";
    for (auto&& [left, right] : zip(x, y)) std::cout << '(' << left << ", " << right << ") ";
    std::cout << '\n';
    auto sums = x % y | std::views::transform([](auto pair) { return pair.first + pair.second; });
    print("Zipped sums", contara::to_vector(sums));
    std::cout << "Cartesian pairs: ";
    for (auto&& [left, right] : x * y) std::cout << '(' << left << ", " << right << ") ";
    std::cout << '\n';
}
