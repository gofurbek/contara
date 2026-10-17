#include <contara/contara.hpp>

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>

namespace {
volatile std::uint64_t observed = 0;

template<class F>
double measure(F operation, int rounds = 5) {
    double best = std::numeric_limits<double>::max();
    for (int trial = 0; trial < 3; ++trial) {
        const auto start = std::chrono::steady_clock::now();
        for (int round = 0; round < rounds; ++round) {
            const auto result = operation();
            observed = observed ^ static_cast<std::uint64_t>(result.size());
            if (!result.empty()) observed = observed ^ static_cast<std::uint64_t>(*result.begin());
        }
        const auto stop = std::chrono::steady_clock::now();
        const auto milliseconds = std::chrono::duration<double, std::milli>(stop - start).count();
        best = std::min(best, milliseconds / rounds);
    }
    return best;
}

std::uint64_t work(std::uint64_t value) {
    for (int i = 0; i < 200; ++i) {
        value ^= value >> 13;
        value *= 0x9e3779b97f4a7c15ULL;
        value ^= value >> 17;
    }
    return value;
}
} // namespace

int main() {
    const auto a = contara::to_set(std::views::iota(0, 20000));
    const auto b = contara::to_set(std::views::iota(10000, 30000));
    const auto stl_union = [&] {
        std::set<int> result;
        std::set_union(a.begin(), a.end(), b.begin(), b.end(), std::inserter(result, result.end()));
        return result;
    };
    const auto short_union = [&] { return contara::unite(a, b); };
    if (stl_union() != short_union()) return 1;

    std::vector<std::uint64_t> values(100000);
    std::iota(values.begin(), values.end(), std::uint64_t{0});
    const auto sequential = [&] {
        std::vector<std::uint64_t> result;
        result.reserve(values.size());
        for (const auto value : values) result.push_back(work(value));
        return result;
    };
    const auto parallel = [&] { return contara::parallel_map(values, work, {.workers = 4}); };
    if (sequential() != parallel()) return 1;
    std::cout << std::fixed << std::setprecision(3)
              << "Milliseconds per operation (best of 3 batches; lower is better)\n"
              << "STL set union:        " << measure(stl_union) << '\n'
              << "Contara set union:    " << measure(short_union) << '\n'
              << "Sequential transform: " << measure(sequential) << '\n'
              << "Parallel map (4):     " << measure(parallel) << '\n'
              << "Timings include allocation and thread startup. Results depend on hardware and workload.\n";
}
