#include <contara/contara.hpp>

#include <array>
#include <cstdint>
#include <forward_list>
#include <iostream>
#include <memory>
#include <memory_resource>
#include <numeric>
#include <random>
#include <sstream>
#include <string>

namespace {
int checks = 0;
#define CHECK(...) do { ++checks; if (!(__VA_ARGS__)) throw std::runtime_error( \
    std::string("line ") + std::to_string(__LINE__) + ": " + #__VA_ARGS__); } while (false)

using namespace contara::ops;

template<class S>
void check_set_algebra() {
    const S a{1, 2, 3, 4}, b{3, 4, 5};
    CHECK((a | b) == S({1, 2, 3, 4, 5}));
    CHECK((a & b) == S({3, 4}));
    CHECK((a - b) == S({1, 2}));
    CHECK((b - a) == S({5}));
    CHECK((a ^ b) == S({1, 2, 5}));
    CHECK((a | S{}) == a);
    CHECK((S{} & a).empty());
    CHECK((a - a).empty());
    CHECK((a ^ a).empty());
    CHECK(a == S({1, 2, 3, 4}));
    auto value = a;
    CHECK(&(value |= b) == &value);
    CHECK(value == (a | b));
    value = a; value &= b; CHECK(value == (a & b));
    value = a; value -= b; CHECK(value == (a - b));
    value = a; value ^= b; CHECK(value == (a ^ b));
    value = a; value |= value; CHECK(value == a);
    value ^= value; CHECK(value.empty());

    // Independent oracle: boolean membership over a small universe.
    std::mt19937 random(42);
    for (int trial = 0; trial < 100; ++trial) {
        S left, right;
        std::array<bool, 32> in_left{}, in_right{};
        for (int i = 0; i < 60; ++i) {
            auto x = random() % 32;
            in_left[x] = true; left.insert(static_cast<int>(x));
            x = random() % 32;
            in_right[x] = true; right.insert(static_cast<int>(x));
        }
        const auto united = left | right;
        const auto common = left & right;
        const auto difference = left - right;
        const auto exclusive = left ^ right;
        for (int x = 0; x < 32; ++x) {
            CHECK(united.contains(x) == (in_left[x] || in_right[x]));
            CHECK(common.contains(x) == (in_left[x] && in_right[x]));
            CHECK(difference.contains(x) == (in_left[x] && !in_right[x]));
            CHECK(exclusive.contains(x) == (in_left[x] != in_right[x]));
        }
    }
}

struct order {
    bool reverse;
    bool operator()(int a, int b) const { return reverse ? a > b : a < b; }
};
struct modulo_order {
    int modulus;
    bool operator()(int a, int b) const { return a % modulus < b % modulus; }
};
struct modulo_hash {
    int modulus;
    std::size_t operator()(int value) const { return std::hash<int>{}(value % modulus); }
};
struct modulo_equal {
    int modulus;
    bool operator()(int a, int b) const { return a % modulus == b % modulus; }
};

void check_policies() {
    const std::set<int, order> a({1, 3}, order{false}), b({2, 3}, order{true});
    CHECK(contara::to_vector(a | b) == std::vector<int>({1, 2, 3}));
    CHECK(contara::to_vector(b | a) == std::vector<int>({3, 2, 1}));
    CHECK(contara::to_vector(a & b) == std::vector<int>({3}));
    CHECK(contara::to_vector(a - b) == std::vector<int>({1}));
    CHECK(contara::to_vector(a ^ b) == std::vector<int>({1, 2}));
    const std::set<int, modulo_order> left({1, 2}, modulo_order{10});
    const std::set<int, modulo_order> right({11, 12, 22}, modulo_order{100});
    CHECK((left | right).size() == 2);
    CHECK((left & right).size() == 2);
    CHECK((left - right).empty());
    CHECK((left ^ right).empty());

    using hash_set = std::unordered_set<int, modulo_hash, modulo_equal>;
    const hash_set hash_left({1, 2}, 0, modulo_hash{10}, modulo_equal{10});
    const hash_set hash_right({11, 12, 22}, 0, modulo_hash{100}, modulo_equal{100});
    CHECK((hash_left | hash_right).size() == 2);
    CHECK((hash_left & hash_right).size() == 2);
    CHECK((hash_left - hash_right).empty());
    CHECK((hash_left ^ hash_right).empty());

    std::pmr::monotonic_buffer_resource resource;
    const std::pmr::set<int> allocated({1, 3}, &resource);
    const std::pmr::set<int> other({2, 3});
    CHECK((allocated | other).get_allocator().resource() == &resource);
    const std::pmr::vector<int> sequence({1, 2}, &resource);
    CHECK(contara::concat(sequence, sequence).get_allocator().resource() == &resource);
    std::pmr::unordered_set<int> hashed({1, 2}, &resource);
    hashed.max_load_factor(0.5f);
    const auto result = hashed | hashed;
    CHECK(result.get_allocator().resource() == &resource);
    CHECK(result.max_load_factor() == 0.5f);
}

template<class S>
void check_sequences() {
    const S a{1, 2}, b{3, 4};
    CHECK((a + b) == S({1, 2, 3, 4}));
    CHECK((a + S{} + b) == S({1, 2, 3, 4}));
    CHECK((S{} + b) == b);
    auto copy = a;
    CHECK(&(copy += b) == &copy);
    CHECK(copy == S({1, 2, 3, 4}));
    copy = a; copy += copy;
    CHECK(copy == S({1, 2, 1, 2}));
    copy = a;
    auto moved = std::move(copy) + copy;
    CHECK(moved == S({1, 2, 1, 2}));
    CHECK(a == S({1, 2}));
}

void check_conversions() {
    const std::array values{3, 1, 3, 2};
    CHECK(contara::to_set(values) == std::set<int>({1, 2, 3}));
    CHECK(contara::to_unordered_set(values) == std::unordered_set<int>({1, 2, 3}));
    CHECK(contara::to_vector(values) == std::vector<int>({3, 1, 3, 2}));
    CHECK(contara::to_deque(values) == std::deque<int>({3, 1, 3, 2}));
    CHECK(contara::to_list(values) == std::list<int>({3, 1, 3, 2}));
    CHECK(contara::to_vector(std::set<int>{1, 2}) == std::vector<int>({1, 2}));
    const std::forward_list<int> linked{4, 5};
    CHECK(contara::to_vector(linked) == std::vector<int>({4, 5}));
    auto evens = values | std::views::filter([](int x) { return x % 2 == 0; });
    CHECK(contara::to_vector(evens) == std::vector<int>({2}));
    CHECK(contara::to_vector(std::views::iota(0) | std::views::take(3)) == std::vector<int>({0, 1, 2}));
    std::istringstream input("7 8 9");
    CHECK(contara::to_vector(std::ranges::istream_view<int>(input)) == std::vector<int>({7, 8, 9}));
    std::vector<std::unique_ptr<int>> owned;
    owned.push_back(std::make_unique<int>(42));
    const auto moved = contara::to_vector(std::ranges::subrange(
        std::make_move_iterator(owned.begin()), std::make_move_iterator(owned.end())));
    CHECK(*moved[0] == 42);
    CHECK(!owned[0]);

    std::vector<int> reusable{1, 2};
    reusable.reserve(16);
    const auto pointer = reusable.data();
    const auto combined = std::move(reusable) + std::vector<int>{3, 4};
    CHECK(combined.data() == pointer);
}

struct non_default {
    explicit non_default(int v) : value(v) {}
    int value;
};

void check_parallel() {
    std::vector<int> values(10003);
    std::iota(values.begin(), values.end(), 0);
    const auto options = contara::parallel_options{.workers = 4, .min_per_worker = 1};
    const auto squares = contara::parallel_map(values, [](int x) { return std::int64_t{x} * x; }, options);
    CHECK(squares.size() == values.size());
    for (std::size_t i = 0; i < values.size(); ++i) CHECK(squares[i] == std::int64_t{values[i]} * values[i]);
    const auto booleans = contara::parallel_map(values, [](int x) { return x % 2 == 0; }, options);
    for (std::size_t i = 0; i < values.size(); ++i) CHECK(booleans[i] == (i % 2 == 0));
    const auto objects = contara::parallel_map(values, [](int x) { return non_default(x); }, options);
    CHECK(objects.back().value == values.back());
    const auto owned = contara::parallel_map(values, [](int x) { return std::make_unique<int>(x); }, options);
    CHECK(*owned.back() == values.back());
    CHECK(contara::parallel_map(std::vector<int>{}, [](int x) { return x; }, options).empty());
    CHECK(contara::parallel_map(std::array{2, 3}, [](int x) { return x + 1; }) == std::vector<int>({3, 4}));
    CHECK(contara::parallel_map(std::deque<int>{2, 3}, [](int x) { return x + 1; }, options) == std::vector<int>({3, 4}));
    CHECK(contara::parallel_map(std::vector<bool>{true, false}, [](bool x) { return !x; }, options) == std::vector<bool>({false, true}));
    CHECK(values.front() == 0 && values.back() == 10002);

    std::mutex mutex;
    std::set<std::thread::id> thread_ids;
    (void)contara::parallel_map(values, [&](int x) {
        std::lock_guard lock(mutex);
        thread_ids.insert(std::this_thread::get_id());
        return x;
    }, options);
    CHECK(thread_ids.size() == 4);

    for (const std::size_t worker_count : {std::size_t{1}, std::size_t{4}}) {
        bool caught = false;
        try {
            (void)contara::parallel_map(values, [](int x) {
                if (x == 10) throw std::runtime_error("callback failure");
                return x;
            }, {.workers = worker_count, .min_per_worker = 1});
        } catch (const std::runtime_error& e) {
            caught = std::string(e.what()) == "callback failure";
        }
        CHECK(caught);
    }
    bool invalid = false;
    try {
        (void)contara::parallel_map(values, [](int x) { return x; }, {.min_per_worker = 0});
    } catch (const std::invalid_argument&) { invalid = true; }
    CHECK(invalid);
}
} // namespace

int main() {
    try {
        check_set_algebra<std::set<int>>();
        check_set_algebra<std::unordered_set<int>>();
        check_policies();
        check_sequences<std::vector<int>>();
        check_sequences<std::deque<int>>();
        check_sequences<std::list<int>>();
        check_conversions();
        check_parallel();
        std::cout << "Passed " << checks << " checks\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
