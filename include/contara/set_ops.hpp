#pragma once

#include <algorithm>
#include <bit>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <memory>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>
#include <utility>

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

template<class S>
S copy_like(const S& source) {
    if constexpr (ordered_set<S>) return S(source, source.get_allocator());
    else {
        auto result = empty_like(source);
        result.reserve(source.size());
        result.insert(source.begin(), source.end());
        return result;
    }
}

// Both operands are interpreted using the left operand's key equivalence.
template<class S, class F>
decltype(auto) with_compatible_rhs(const S& a, const S& b, F operation) {
    bool compatible;
    if constexpr (ordered_set<S>) {
        compatible = same_policy(a.key_comp(), b.key_comp());
    } else {
        // Different hash seeds are safe: membership is defined by key_eq.
        compatible = same_policy(a.key_eq(), b.key_eq());
    }
    if (compatible) {
        return operation(b);
    }
    if constexpr (std::is_copy_constructible_v<typename S::value_type>) {
        auto normalized = empty_like(a);
        if constexpr (hashed_set<S>) normalized.reserve(b.size());
        normalized.insert(b.begin(), b.end());
        return operation(normalized);
    } else {
        throw std::invalid_argument("contara: noncopyable keys require compatible policies");
    }
}

enum class set_operation { unite, intersect, difference, symmetric_difference };

inline bool prefer_lookup(std::size_t small, std::size_t large) {
    const auto height = std::max(std::size_t{1}, static_cast<std::size_t>(std::bit_width(large)));
    return small <= large / height;
}

template<set_operation Op, class S>
S combine(const S& a, const S& b) {
    if (std::addressof(a) == std::addressof(b)) {
        if constexpr (Op == set_operation::difference || Op == set_operation::symmetric_difference) {
            return empty_like(a);
        } else {
            return copy_like(a);
        }
    }
    if constexpr (Op == set_operation::intersect || Op == set_operation::difference) {
        if (a.empty()) return empty_like(a);
    }
    if constexpr (Op == set_operation::intersect) {
        if (b.empty()) return empty_like(a);
    } else {
        if (b.empty()) return copy_like(a);
    }
    return with_compatible_rhs(a, b, [&](const S& rhs) {
        auto result = empty_like(a);
        if constexpr (ordered_set<S>) {
            auto out = std::inserter(result, result.end());
            auto comp = a.key_comp();
            if constexpr (Op == set_operation::unite) {
                std::set_union(a.begin(), a.end(), rhs.begin(), rhs.end(), out, comp);
            } else if constexpr (Op == set_operation::intersect) {
                if (comp(*a.rbegin(), *rhs.begin()) || comp(*rhs.rbegin(), *a.begin())) return result;
                if (prefer_lookup(std::min(a.size(), rhs.size()), std::max(a.size(), rhs.size()))) {
                    if (a.size() <= rhs.size()) {
                        for (const auto& value : a) {
                            if (rhs.contains(value)) result.insert(result.end(), value);
                        }
                    } else {
                        for (const auto& value : rhs) {
                            const auto found = a.find(value);
                            if (found != a.end()) result.insert(result.end(), *found);
                        }
                    }
                } else {
                    std::set_intersection(a.begin(), a.end(), rhs.begin(), rhs.end(), out, comp);
                }
            } else if constexpr (Op == set_operation::difference) {
                if (a.size() <= rhs.size() && prefer_lookup(a.size(), rhs.size())) {
                    for (const auto& value : a) {
                        if (!rhs.contains(value)) result.insert(result.end(), value);
                    }
                } else {
                    std::set_difference(a.begin(), a.end(), rhs.begin(), rhs.end(), out, comp);
                }
            } else {
                std::set_symmetric_difference(a.begin(), a.end(), rhs.begin(), rhs.end(), out, comp);
            }
        } else {
            if constexpr (Op == set_operation::unite) {
                const auto bound = rhs.size() <= result.max_size() - a.size()
                    ? a.size() + rhs.size() : std::max(a.size(), rhs.size());
                result.reserve(bound);
                result.insert(a.begin(), a.end());
                result.insert(rhs.begin(), rhs.end());
            } else if constexpr (Op == set_operation::intersect) {
                if (a.size() <= rhs.size()) {
                    for (const auto& value : a) {
                        if (rhs.contains(value)) result.insert(value);
                    }
                } else {
                    for (const auto& value : rhs) {
                        const auto found = a.find(value);
                        if (found != a.end()) result.insert(*found);
                    }
                }
            } else {
                for (const auto& value : a) {
                    if (!rhs.contains(value)) result.insert(value);
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

template<set_operation Op, class S>
S& combine_into(S& a, const S& b) {
    if (std::addressof(a) == std::addressof(b)) {
        if constexpr (Op == set_operation::difference || Op == set_operation::symmetric_difference) a.clear();
        return a;
    }
    if (b.empty()) {
        if constexpr (Op == set_operation::intersect) a.clear();
        return a;
    }
    if constexpr (Op == set_operation::intersect || Op == set_operation::difference) {
        if (a.empty()) return a;
    }
    return with_compatible_rhs(a, b, [&](const S& rhs) -> S& {
        if constexpr (ordered_set<S>) {
            if constexpr (Op == set_operation::intersect || Op == set_operation::difference) {
                if (prefer_lookup(a.size(), rhs.size())) {
                    for (auto it = a.begin(); it != a.end();) {
                        const bool present = rhs.contains(*it);
                        if (present == (Op == set_operation::difference)) it = a.erase(it);
                        else ++it;
                    }
                    return a;
                }
            }
            if constexpr (Op != set_operation::intersect) {
                if (prefer_lookup(rhs.size(), a.size())) {
                    for (const auto& value : rhs) {
                        if constexpr (Op == set_operation::unite) {
                            a.insert(value);
                        } else if constexpr (Op == set_operation::difference) {
                            a.erase(value);
                        } else {
                            const auto found = a.find(value);
                            if (found == a.end()) a.insert(value);
                            else a.erase(found);
                        }
                    }
                    return a;
                }
            }
            const auto comp = a.key_comp();
            auto left = a.begin();
            auto right = rhs.begin();
            while (left != a.end() && right != rhs.end()) {
                if (comp(*left, *right)) {
                    if constexpr (Op == set_operation::intersect) left = a.erase(left);
                    else ++left;
                } else if (comp(*right, *left)) {
                    if constexpr (Op == set_operation::unite || Op == set_operation::symmetric_difference) {
                        a.insert(left, *right);
                    }
                    ++right;
                } else {
                    if constexpr (Op == set_operation::difference || Op == set_operation::symmetric_difference) {
                        left = a.erase(left);
                    } else ++left;
                    ++right;
                }
            }
            if constexpr (Op == set_operation::intersect) {
                a.erase(left, a.end());
            } else if constexpr (Op == set_operation::unite || Op == set_operation::symmetric_difference) {
                for (; right != rhs.end(); ++right) a.insert(a.end(), *right);
            }
        } else {
            if constexpr (Op == set_operation::unite) {
                a.insert(rhs.begin(), rhs.end());
            } else if constexpr (Op == set_operation::symmetric_difference) {
                for (const auto& value : rhs) {
                    const auto found = a.find(value);
                    if (found == a.end()) a.insert(value);
                    else a.erase(found);
                }
            } else if constexpr (Op == set_operation::difference) {
                if (rhs.size() < a.size()) {
                    for (const auto& value : rhs) a.erase(value);
                } else {
                    for (auto it = a.begin(); it != a.end();) {
                        if (rhs.contains(*it)) it = a.erase(it);
                        else ++it;
                    }
                }
            } else {
                for (auto it = a.begin(); it != a.end();) {
                    if (!rhs.contains(*it)) it = a.erase(it);
                    else ++it;
                }
            }
        }
        return a;
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

template<set_container S> S& unite_into(S& a, const S& b) {
    return detail::combine_into<detail::set_operation::unite>(a, b);
}
template<set_container S> S& intersect_into(S& a, const S& b) {
    return detail::combine_into<detail::set_operation::intersect>(a, b);
}
template<set_container S> S& difference_into(S& a, const S& b) {
    return detail::combine_into<detail::set_operation::difference>(a, b);
}
template<set_container S> S& symmetric_difference_into(S& a, const S& b) {
    return detail::combine_into<detail::set_operation::symmetric_difference>(a, b);
}

template<set_container S> [[nodiscard]] S unite(S&& a, const S& b) { unite_into(a, b); return std::move(a); }
template<set_container S> [[nodiscard]] S intersect(S&& a, const S& b) { intersect_into(a, b); return std::move(a); }
template<set_container S> [[nodiscard]] S difference(S&& a, const S& b) { difference_into(a, b); return std::move(a); }
template<set_container S> [[nodiscard]] S symmetric_difference(S&& a, const S& b) { symmetric_difference_into(a, b); return std::move(a); }

// Opt in locally with `using namespace contara::ops;`.
namespace ops {
template<set_container S> [[nodiscard]] S operator|(const S& a, const S& b) { return unite(a, b); }
template<set_container S> [[nodiscard]] S operator&(const S& a, const S& b) { return intersect(a, b); }
template<set_container S> [[nodiscard]] S operator-(const S& a, const S& b) { return difference(a, b); }
template<set_container S> [[nodiscard]] S operator^(const S& a, const S& b) { return symmetric_difference(a, b); }
template<set_container S> [[nodiscard]] S operator|(S&& a, const S& b) { return unite(std::move(a), b); }
template<set_container S> [[nodiscard]] S operator&(S&& a, const S& b) { return intersect(std::move(a), b); }
template<set_container S> [[nodiscard]] S operator-(S&& a, const S& b) { return difference(std::move(a), b); }
template<set_container S> [[nodiscard]] S operator^(S&& a, const S& b) { return symmetric_difference(std::move(a), b); }
template<set_container S> S& operator|=(S& a, const S& b) { return unite_into(a, b); }
template<set_container S> S& operator&=(S& a, const S& b) { return intersect_into(a, b); }
template<set_container S> S& operator-=(S& a, const S& b) { return difference_into(a, b); }
template<set_container S> S& operator^=(S& a, const S& b) { return symmetric_difference_into(a, b); }
} // namespace ops
} // namespace contara
