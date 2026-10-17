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
        auto changed = left;
        changed |= right; CHECK(changed == united);
        changed = left; changed &= right; CHECK(changed == common);
        changed = left; changed -= right; CHECK(changed == difference);
        changed = left; changed ^= right; CHECK(changed == exclusive);
        changed = left; CHECK((std::move(changed) | right) == united);
        changed = left; CHECK((std::move(changed) & right) == common);
        changed = left; CHECK((std::move(changed) - right) == difference);
        changed = left; CHECK((std::move(changed) ^ right) == exclusive);
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
    auto changed = left;
    changed |= right; CHECK(changed.size() == 2);
    changed = left; changed &= right; CHECK(changed.size() == 2);
    changed = left; changed -= right; CHECK(changed.empty());
    changed = left; changed ^= right; CHECK(changed.empty());
    auto changed_hash = hash_left;
    changed_hash |= hash_right; CHECK(changed_hash.size() == 2);
    changed_hash = hash_left; changed_hash &= hash_right; CHECK(changed_hash.size() == 2);
    changed_hash = hash_left; changed_hash -= hash_right; CHECK(changed_hash.empty());
    changed_hash = hash_left; changed_hash ^= hash_right; CHECK(changed_hash.empty());

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
    copy = a; copy += std::move(copy); CHECK(copy == S({1, 2, 1, 2}));
    copy = a;
    auto right = b;
    CHECK((std::move(copy) + std::move(right)) == S({1, 2, 3, 4}));
}

struct counting_resource : std::pmr::memory_resource {
    std::size_t allocations = 0;
    void* do_allocate(std::size_t size, std::size_t alignment) override {
        ++allocations;
        return std::pmr::new_delete_resource()->allocate(size, alignment);
    }
    void do_deallocate(void* pointer, std::size_t size, std::size_t alignment) override {
        std::pmr::new_delete_resource()->deallocate(pointer, size, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override { return this == &other; }
};

struct counted_less {
    std::size_t* calls;
    bool operator()(int a, int b) const { ++*calls; return a < b; }
    bool operator==(const counted_less&) const { return true; }
};
struct counted_hash {
    std::size_t* calls;
    std::size_t seed;
    std::size_t operator()(int value) const { ++*calls; return std::hash<int>{}(value) ^ seed; }
};
struct record { int key; int payload; };
struct record_order {
    bool operator()(const record& a, const record& b) const { return a.key < b.key; }
};
struct record_hash { std::size_t operator()(const record& value) const { return std::hash<int>{}(value.key); } };
struct record_equal { bool operator()(const record& a, const record& b) const { return a.key == b.key; } };
struct collision_hash { std::size_t operator()(int) const { return 0; } };
struct counted_value {
    int value;
    static inline std::size_t comparisons = 0;
    bool operator<(const counted_value& other) const { ++comparisons; return value < other.value; }
};

void check_performance_contracts() {
    std::size_t comparisons = 0;
    std::set<int, counted_less> large(counted_less{&comparisons}), tiny(counted_less{&comparisons});
    for (int i = 0; i < 8192; ++i) large.insert(large.end(), i);
    tiny.insert(8000);
    comparisons = 0;
    CHECK((large & tiny).size() == 1);
    CHECK(comparisons < 128);
    comparisons = 0;
    CHECK((tiny - large).empty());
    CHECK(comparisons < 128);
    auto tiny_copy = tiny;
    comparisons = 0;
    tiny_copy &= large;
    CHECK(tiny_copy.size() == 1 && comparisons < 128);

    std::size_t hashes = 0;
    std::unordered_set<int, counted_hash> hash_large(0, counted_hash{&hashes, 1});
    std::unordered_set<int, counted_hash> hash_tiny(0, counted_hash{&hashes, 777});
    for (int i = 0; i < 8192; ++i) hash_large.insert(i);
    hash_tiny.insert(8000);
    hashes = 0;
    CHECK((hash_large & hash_tiny).size() == 1);
    CHECK(hashes < 32);
    hashes = 0;
    CHECK((hash_tiny & hash_large).size() == 1);
    CHECK(hashes < 32);

    std::vector<counted_value> sorted;
    for (int i = 0; i < 8192; ++i) sorted.push_back({i});
    counted_value::comparisons = 0;
    CHECK(contara::to_set(sorted).size() == sorted.size());
    CHECK(counted_value::comparisons < 8 * sorted.size());
    auto stream = std::views::iota(0) | std::views::take(16384) |
        std::views::transform([](int x) { return counted_value{x / 2}; });
    counted_value::comparisons = 0;
    CHECK(contara::to_set(stream).size() == 8192);
    CHECK(counted_value::comparisons < 8 * 16384);

    counting_resource resource;
    std::pmr::set<int> a({1, 2, 3}, &resource), b({2, 3}, &resource);
    const auto kept = std::addressof(*a.find(2));
    resource.allocations = 0;
    a &= b;
    CHECK(resource.allocations == 0);
    CHECK(std::addressof(*a.find(2)) == kept);
    a -= b;
    CHECK(a.empty() && resource.allocations == 0);
    a.insert(1); b.clear(); b.insert(2);
    resource.allocations = 0;
    a |= b;
    CHECK(a.size() == 2 && resource.allocations == 1);

    std::pmr::vector<int> repeated(&resource);
    const std::pmr::vector<int> one({7}, &resource);
    resource.allocations = 0;
    for (int i = 0; i < 4096; ++i) repeated += one;
    CHECK(repeated.size() == 4096);
    CHECK(resource.allocations < 20);
    repeated.reserve(8192);
    resource.allocations = 0;
    repeated += repeated;
    CHECK(repeated.size() == 8192 && resource.allocations == 0);

    std::list<int> nodes{1, 2}, extra{3, 4};
    const auto node_address = std::addressof(extra.front());
    nodes += std::move(extra);
    CHECK(extra.empty() && std::addressof(*std::next(nodes.begin(), 2)) == node_address);
    std::pmr::list<int> list_left({1}, &resource);
    resource.allocations = 0;
    list_left += list_left;
    CHECK(list_left.size() == 2 && resource.allocations == 1);
    std::pmr::monotonic_buffer_resource other_resource;
    std::pmr::list<int> list_right({3}, &other_resource);
    list_left += std::move(list_right);
    CHECK(list_left.back() == 3);

    std::vector<std::unique_ptr<int>> owned, more;
    owned.push_back(std::make_unique<int>(1));
    more.push_back(std::make_unique<int>(2));
    auto moved = std::move(owned) + std::move(more);
    CHECK(moved.size() == 2 && *moved[0] == 1 && *moved[1] == 2);
    const auto address = moved.data();
    const auto converted = contara::to_vector(std::move(moved));
    CHECK(converted.data() == address);
    std::set<int> transferable{1, 2};
    const auto set_address = std::addressof(*transferable.begin());
    const auto transferred = contara::to_set(std::move(transferable));
    CHECK(std::addressof(*transferred.begin()) == set_address);
    CHECK(contara::to_set(std::vector<int>{1, 1, 2, 2}) == std::set<int>({1, 2}));

    const std::set<record, record_order> records{{1, 10}, {2, 20}, {3, 30}, {4, 40}};
    const std::set<record, record_order> selected{{3, 999}};
    CHECK((records & selected).begin()->payload == 30);
    const std::unordered_set<record, record_hash, record_equal> hashed_records{{1, 10}, {2, 20}, {3, 30}};
    const std::unordered_set<record, record_hash, record_equal> hashed_selected{{3, 999}};
    CHECK((hashed_records & hashed_selected).begin()->payload == 30);
    std::vector<bool> bits{true, false};
    bits += bits;
    CHECK(bits == std::vector<bool>({true, false, true, false}));
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
struct non_assignable {
    explicit non_assignable(int v) : value(v) {}
    non_assignable(non_assignable&&) = default;
    non_assignable& operator=(non_assignable&&) = delete;
    int value;
};
struct tracked_result {
    static inline std::atomic<int> live{0};
    int value;
    explicit tracked_result(int v) : value(v) { ++live; }
    tracked_result(tracked_result&& other) noexcept : value(other.value) { ++live; }
    tracked_result& operator=(tracked_result&&) = delete;
    ~tracked_result() { --live; }
};

constexpr bool zip_constexpr_check() {
    std::array a{1, 2, 3};
    std::array b{10, 20};
    auto pairs = contara::zip(a, b);
    int total = 0;
    for (auto [x, y] : pairs) total += x + y;
    return pairs.size() == 2 && total == 33 && pairs[1].second == 20;
}
static_assert(zip_constexpr_check());

void check_zip() {
    std::vector<int> a{1, 2, 3};
    std::array b{10, 20};
    auto pairs = contara::zip(a, b);
    static_assert(std::ranges::view<decltype(pairs)>);
    static_assert(std::ranges::random_access_range<decltype(pairs)>);
    static_assert(std::ranges::common_range<decltype(pairs)>);
    static_assert(std::ranges::borrowed_range<decltype(pairs)>);
    static_assert(std::same_as<std::ranges::range_value_t<decltype(pairs)>, contara::zip_pair<int, int>>);
    CHECK(pairs.size() == 2 && pairs.end() - pairs.begin() == 2);
    CHECK(pairs[1].first == 2 && pairs[1].second == 20);
    auto it = pairs.begin();
    CHECK((*(it++)).first == 1);
    CHECK((*it).first == 2);
    CHECK((it - 1) == pairs.begin() && (1 + pairs.begin()) == it);
    CHECK(pairs.begin() < it && it > pairs.begin() && it <= pairs.end() && it >= pairs.begin());
    CHECK((*--it).first == 1);
    CHECK((it += 1) == pairs.begin() + 1);
    CHECK((it -= 1) == pairs.begin());

    for (auto&& [x, y] : pairs) x += y;
    CHECK(a == std::vector<int>({11, 22, 3}));
    const auto values = contara::to_vector(pairs);
    CHECK(values == std::vector<contara::zip_pair<int, int>>({{11, 10}, {22, 20}}));
    std::vector<contara::zip_pair<int, int>> copied;
    std::ranges::copy(pairs, std::back_inserter(copied));
    CHECK(copied == values);
    a[0] = 99;
    CHECK(values[0].first == 11);
    CHECK(contara::to_set(pairs).begin()->first == 22);
    auto sortable = contara::to_vector(pairs);
    std::ranges::sort(sortable);
    CHECK(sortable.front().first == 22);
    const auto nested = contara::to_vector(contara::zip(pairs, std::views::iota(0, 2)));
    CHECK(nested[0].first.first == 99 && nested[1].second == 1);
    CHECK(contara::to_vector(pairs | std::views::transform([](auto p) { return p.first + p.second; })) == std::vector<int>({109, 42}));
    auto selected = pairs | std::views::filter([](auto p) { return p.first < 50; });
    CHECK(contara::to_vector(selected).size() == 1);
    const auto& const_pairs = pairs;
    CHECK(const_pairs.size() == 2 && (*const_pairs.begin()).second == 10);
    const std::array immutable{5, 6};
    auto read_only = contara::zip(immutable, b);
    static_assert(std::same_as<decltype((*read_only.begin()).first), const int&>);
    CHECK((*read_only.begin()).first == 5);

    auto owned = contara::zip(std::vector<int>{4, 5}, std::vector<int>{6, 7, 8});
    static_assert(!std::ranges::borrowed_range<decltype(owned)>);
    CHECK(owned.size() == 2 && owned[1].second == 7);
    CHECK(contara::to_vector(contara::zip(a, std::vector<int>{})).empty());
    CHECK(contara::to_vector(contara::zip(std::vector<int>{}, a)).empty());
    auto unbounded = contara::zip(std::views::iota(0), b);
    CHECK(contara::to_vector(unbounded) == std::vector<contara::zip_pair<int, int>>({{0, 10}, {1, 20}}));
    auto infinite = contara::zip(std::views::iota(0), std::views::iota(100)) | std::views::take(3);
    CHECK(contara::to_vector(infinite).back().second == 102);

    std::list<int> linked{2, 4, 6};
    auto linked_pairs = contara::zip(linked, b);
    static_assert(std::ranges::bidirectional_range<decltype(linked_pairs)>);
    CHECK(contara::to_vector(linked_pairs).size() == 2);
    CHECK(contara::to_vector(linked_pairs | std::views::reverse)[0].first == 4);
    std::forward_list<int> forward{3, 6, 9};
    auto forward_pairs = contara::zip(forward, b);
    static_assert(std::ranges::forward_range<decltype(forward_pairs)>);
    CHECK(contara::to_vector(forward_pairs)[1].first == 6);
    std::istringstream input("7 8 9");
    auto streamed = contara::zip(std::ranges::istream_view<int>(input), b);
    static_assert(std::ranges::input_range<decltype(streamed)>);
    static_assert(!std::ranges::forward_range<decltype(streamed)>);
    CHECK(contara::to_vector(streamed) == std::vector<contara::zip_pair<int, int>>({{7, 10}, {8, 20}}));

    std::vector<bool> bits{true, false};
    auto bit_pairs = contara::zip(bits, b);
    static_assert(std::ranges::input_range<decltype(bit_pairs)>);
    (*bit_pairs.begin()).first = false;
    CHECK(!bits[0] && contara::to_vector(bit_pairs)[1].second == 20);

    std::vector<std::unique_ptr<int>> pointers;
    pointers.push_back(std::make_unique<int>(42));
    auto pointer_pairs = contara::zip(pointers, b);
    static_assert(std::ranges::input_range<decltype(pointer_pairs)>);
    auto dereferenced = pointer_pairs | std::views::transform([](auto p) { return *p.first + p.second; });
    CHECK(contara::to_vector(dereferenced) == std::vector<int>({52}));
    const auto moved = contara::to_vector(std::ranges::subrange(std::make_move_iterator(pointer_pairs.begin()), std::make_move_iterator(pointer_pairs.end())));
    CHECK(*moved[0].first == 42 && !pointers[0]);

    std::ranges::iter_swap(pairs.begin(), pairs.begin() + 1);
    CHECK(a[0] == 22 && b[0] == 20);
    counting_resource resource;
    std::pmr::vector<int> first({1, 2, 3}, &resource), second({4, 5}, &resource);
    resource.allocations = 0;
    int total = 0;
    for (auto&& [x, y] : contara::zip(first, second)) total += x + y;
    CHECK(total == 12 && resource.allocations == 0);
}

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
    const auto unassignable = contara::parallel_map(values, [](int x) { return non_assignable(x); }, options);
    CHECK(unassignable.back().value == values.back());
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
    bool generic_failure = false;
    try {
        (void)contara::parallel_map(values, [](int x) {
            if (x == 100) throw std::runtime_error("generic failure");
            return tracked_result(x);
        }, options);
    } catch (const std::runtime_error&) { generic_failure = true; }
    CHECK(generic_failure && tracked_result::live == 0);
}
} // namespace

int main() {
    try {
        check_set_algebra<std::set<int>>();
        check_set_algebra<std::unordered_set<int>>();
        check_set_algebra<std::unordered_set<int, collision_hash>>();
        check_policies();
        check_sequences<std::vector<int>>();
        check_sequences<std::deque<int>>();
        check_sequences<std::list<int>>();
        check_conversions();
        check_performance_contracts();
        check_parallel();
        check_zip();
        std::cout << "Passed " << checks << " checks\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
