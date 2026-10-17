#include <contara/contara.hpp>

int main() {
    using namespace contara::ops;
    const auto common = std::set<int>{1, 2, 3} & std::set<int>{2, 3, 4};
    const auto values = contara::to_vector(common);
    const auto output = contara::parallel_map(values, [](int value) { return value + 1; },
        {.workers = 2, .min_per_worker = 1});
    const auto paired = contara::to_vector(contara::zip(values, output));
    return output == std::vector<int>{3, 4} && paired.size() == 2 && paired[0].first == 2 && paired[0].second == 3 ? 0 : 1;
}
