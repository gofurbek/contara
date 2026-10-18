#include <contara/contara.hpp>

int main() {
    using namespace contara::ops;
    const auto common = std::set<int>{1, 2, 3} & std::set<int>{2, 3, 4};
    const auto values = contara::to_vector(common);
    const auto output = contara::parallel_map(values, [](int value) { return value + 1; },
        {.workers = 2, .min_per_worker = 1});
    const auto paired = contara::to_vector(contara::zip(values, output));
    const auto zipped = contara::to_vector(values % output);
    const auto product = contara::to_vector(values * output);
    return output == std::vector<int>{3, 4} && paired == zipped && paired.size() == 2 &&
        paired[0].first == 2 && paired[0].second == 3 && product.size() == 4 &&
        product[3].first == 3 && product[3].second == 4 ? 0 : 1;
}
