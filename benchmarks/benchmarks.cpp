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
            auto checksum = static_cast<std::uint64_t>(result.size());
            for (const auto& value : result)
                checksum = (checksum ^ static_cast<std::uint64_t>(value)) * 0x9e3779b97f4a7c15ULL;
            observed = observed ^ checksum;
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

template<class Reference, class Library>
void compare(const char* label, Reference reference, Library library, int rounds = 5) {
    if (reference() != library()) throw std::runtime_error("benchmark outputs differ");
    std::cout << label << "\n  STL/reference: " << measure(reference, rounds)
              << " ms\n  Contara:       " << measure(library, rounds) << " ms\n";
}
} // namespace

int main() {
    using contara::ops::operator%;
    using contara::ops::operator*;
    const auto a = contara::to_set(std::views::iota(0, 20000));
    const auto b = contara::to_set(std::views::iota(10000, 30000));
    const auto stl_union = [&] {
        std::set<int> result;
        std::set_union(a.begin(), a.end(), b.begin(), b.end(), std::inserter(result, result.end()));
        return result;
    };
    const auto short_union = [&] { return contara::unite(a, b); };
    std::cout << std::fixed << std::setprecision(6)
              << "Milliseconds per operation (best of 3 batches; lower is better)\n";
    compare("Ordered union", stl_union, short_union);

    const std::set<int> tiny{19999};
    compare("Ordered intersection, 20000 versus 1", [&] {
        std::set<int> result;
        std::set_intersection(a.begin(), a.end(), tiny.begin(), tiny.end(), std::inserter(result, result.end()));
        return result;
    }, [&] { return contara::intersect(a, tiny); }, 100);

    const auto hashed = contara::to_unordered_set(a);
    const std::unordered_set<int> hash_tiny{19999};
    compare("Unordered intersection, 20000 versus 1", [&] {
        std::unordered_set<int> result;
        for (const auto value : hash_tiny) if (hashed.contains(value)) result.insert(value);
        return result;
    }, [&] { return contara::intersect(hashed, hash_tiny); }, 100);

    const auto sorted = contara::to_vector(a);
    compare("Sorted vector to ordered set", [&] { return std::set<int>(sorted.begin(), sorted.end()); },
        [&] { return contara::to_set(sorted); });

    const std::vector<int> one{7};
    compare("4096 single-element vector appends", [&] {
        std::vector<int> result;
        for (int i = 0; i < 4096; ++i) result.insert(result.end(), one.begin(), one.end());
        return result;
    }, [&] {
        std::vector<int> result;
        for (int i = 0; i < 4096; ++i) contara::append(result, one);
        return result;
    });

    compare("32 ordered-set removals with restoration (includes initial copy)", [&] {
        auto result = a;
        for (int i = 0; i < 32; ++i) { result.erase(19999); result.insert(19999); }
        return result;
    }, [&] {
        using namespace contara::ops;
        auto result = a;
        for (int i = 0; i < 32; ++i) { result -= tiny; result.insert(19999); }
        return result;
    });

    std::vector<std::uint64_t> values(100000);
    std::iota(values.begin(), values.end(), std::uint64_t{0});
    const std::vector<std::uint64_t> offsets(values.size(), 17);
    compare("Pairwise sums: indexed loop versus lazy zip", [&] {
        std::vector<std::uint64_t> result;
        result.reserve(values.size());
        for (std::size_t i = 0; i < values.size(); ++i) result.push_back(values[i] + offsets[i]);
        return result;
    }, [&] {
        auto sums = values % offsets | std::views::transform([](auto pair) { return pair.first + pair.second; });
        return contara::to_vector(sums);
    });
    const auto product_left = contara::to_vector(std::views::iota(std::uint64_t{0}, std::uint64_t{300}));
    const auto product_right = contara::to_vector(std::views::iota(std::uint64_t{0}, std::uint64_t{400}));
    compare("Cartesian sums: nested loops versus lazy product", [&] {
        std::vector<std::uint64_t> result;
        result.reserve(product_left.size() * product_right.size());
        for (const auto x : product_left) for (const auto y : product_right) result.push_back(x + y);
        return result;
    }, [&] {
        auto sums = product_left * product_right | std::views::transform([](auto pair) { return pair.first + pair.second; });
        return contara::to_vector(sums);
    });
    const auto sequential = [&] {
        std::vector<std::uint64_t> result;
        result.reserve(values.size());
        for (const auto value : values) result.push_back(work(value));
        return result;
    };
    const auto parallel = [&] { return contara::parallel_map(values, work, {.workers = 4}); };
    compare("CPU-heavy map (Contara uses 4 workers)", sequential, parallel);
    std::cout << "Timings include allocation, destruction, full-output checksums, and thread startup. Results depend on hardware and workload.\n";
}
