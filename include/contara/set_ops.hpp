#pragma once

#include <algorithm>
#include <concepts>
#include <iterator>
#include <set>
#include <type_traits>
#include <unordered_set>

namespace contara {
namespace detail {

template<class S> inline constexpr bool ordered_set = false;
template<class T, class C, class A>
inline constexpr bool ordered_set<std::set<T, C, A>> = true;
template<class S> inline constexpr bool hashed_set = false;
template<class T, class H, class E, class A>
inline constexpr bool hashed_set<std::unordered_set<T, H, E, A>> = true;

template<class P>
bool same_policy(const P& a, const P& b) {
    if constexpr (std::is_empty_v<P>) {
        return true;
    } else if constexpr (requires { { a == b } -> std::convertible_to<bool>; }) {
        return a == b;
    } else {
        return false;
    }
}

template<class S>
S empty_like(const S& source) {
    if constexpr (ordered_set<S>) {
        return S(source.key_comp(), source.get_allocator());
    } else {
        S result(0, source.hash_function(), source.key_eq(), source.get_allocator());
        result.max_load_factor(source.max_load_factor());
        return result;
    }
}

// Both operands are interpreted using the left operand's key equivalence.
template<class S, class F>
S with_compatible_rhs(const S& a, const S& b, F operation) {
    bool compatible;
    if constexpr (ordered_set<S>) {
        compatible = same_policy(a.key_comp(), b.key_comp());
    } else {
        compatible = same_policy(a.hash_function(), b.hash_function()) &&
                     same_policy(a.key_eq(), b.key_eq());
    }
    if (compatible) {
        return operation(b);
    }
    auto normalized = empty_like(a);
    if constexpr (hashed_set<S>) {
        normalized.reserve(b.size());
    }
    normalized.insert(b.begin(), b.end());
    return operation(normalized);
}

enum class set_operation { unite, intersect, difference, symmetric_difference };

template<set_operation Op, class S>
S combine(const S& a, const S& b) {
    return with_compatible_rhs(a, b, [&](const S& rhs) {
        auto result = empty_like(a);
        if constexpr (ordered_set<S>) {
            auto out = std::inserter(result, result.end());
            auto comp = a.key_comp();
            if constexpr (Op == set_operation::unite) {
                std::set_union(a.begin(), a.end(), rhs.begin(), rhs.end(), out, comp);
            } else if constexpr (Op == set_operation::intersect) {
                std::set_intersection(a.begin(), a.end(), rhs.begin(), rhs.end(), out, comp);
            } else if constexpr (Op == set_operation::difference) {
                std::set_difference(a.begin(), a.end(), rhs.begin(), rhs.end(), out, comp);
            } else {
                std::set_symmetric_difference(a.begin(), a.end(), rhs.begin(), rhs.end(), out, comp);
            }
        } else {
            // Avoid reserving n+m: an intersection may be tiny or empty.
            if constexpr (Op == set_operation::unite) {
                result.reserve(std::max(a.size(), rhs.size()));
                result.insert(a.begin(), a.end());
                result.insert(rhs.begin(), rhs.end());
            } else {
                for (const auto& value : a) {
                    const bool present = rhs.contains(value);
                    if constexpr (Op == set_operation::intersect) {
                        if (present) result.insert(value);
                    } else {
                        if (!present) result.insert(value);
                    }
                }
                if constexpr (Op == set_operation::symmetric_difference) {
                    for (const auto& value : rhs) {
                        if (!a.contains(value)) result.insert(value);
                    }
                }
            }
        }
        return result;
    });
}

} // namespace detail

template<class S>
concept set_container = detail::ordered_set<S> || detail::hashed_set<S>;

template<set_container S>
[[nodiscard]] S unite(const S& a, const S& b) {
    return detail::combine<detail::set_operation::unite>(a, b);
}
template<set_container S>
[[nodiscard]] S intersect(const S& a, const S& b) {
    return detail::combine<detail::set_operation::intersect>(a, b);
}
template<set_container S>
[[nodiscard]] S difference(const S& a, const S& b) {
    return detail::combine<detail::set_operation::difference>(a, b);
}
template<set_container S>
[[nodiscard]] S symmetric_difference(const S& a, const S& b) {
    return detail::combine<detail::set_operation::symmetric_difference>(a, b);
}

// Opt in locally with `using namespace contara::ops;`.
namespace ops {
template<set_container S> [[nodiscard]] S operator|(const S& a, const S& b) { return unite(a, b); }
template<set_container S> [[nodiscard]] S operator&(const S& a, const S& b) { return intersect(a, b); }
template<set_container S> [[nodiscard]] S operator-(const S& a, const S& b) { return difference(a, b); }
template<set_container S> [[nodiscard]] S operator^(const S& a, const S& b) { return symmetric_difference(a, b); }
template<set_container S> S& operator|=(S& a, const S& b) { return a = unite(a, b); }
template<set_container S> S& operator&=(S& a, const S& b) { return a = intersect(a, b); }
template<set_container S> S& operator-=(S& a, const S& b) { return a = difference(a, b); }
template<set_container S> S& operator^=(S& a, const S& b) { return a = symmetric_difference(a, b); }
} // namespace ops
} // namespace contara
