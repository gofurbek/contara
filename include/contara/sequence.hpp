#pragma once

#include <deque>
#include <list>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace contara {
namespace detail {
template<class S> inline constexpr bool sequence = false;
template<class T, class A> inline constexpr bool sequence<std::vector<T, A>> = true;
template<class T, class A> inline constexpr bool sequence<std::deque<T, A>> = true;
template<class T, class A> inline constexpr bool sequence<std::list<T, A>> = true;

template<class S>
void check_append_size(const S& a, const S& b) {
    if (b.size() > a.max_size() - a.size()) {
        throw std::length_error("contara: concatenation exceeds max_size");
    }
}
} // namespace detail

template<class S> concept sequence_container = detail::sequence<S>;

template<sequence_container S>
S& append(S& a, const S& b) {
    detail::check_append_size(a, b);
    if (std::addressof(a) == std::addressof(b)) {
        const S copy(b, b.get_allocator());
        return append(a, copy);
    }
    if constexpr (requires { a.reserve(a.size() + b.size()); }) {
        a.reserve(a.size() + b.size());
    }
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

template<sequence_container S>
[[nodiscard]] S concat(const S& a, const S& b) {
    detail::check_append_size(a, b);
    S result(a.get_allocator());
    if constexpr (requires { result.reserve(a.size() + b.size()); }) {
        result.reserve(a.size() + b.size());
    }
    result.insert(result.end(), a.begin(), a.end());
    result.insert(result.end(), b.begin(), b.end());
    return result;
}

namespace ops {
template<sequence_container S>
[[nodiscard]] S operator+(const S& a, const S& b) { return concat(a, b); }

template<sequence_container S>
[[nodiscard]] S operator+(S&& a, const S& b) {
    append(a, b);
    return std::move(a);
}

template<sequence_container S>
S& operator+=(S& a, const S& b) { return append(a, b); }
} // namespace ops
} // namespace contara
