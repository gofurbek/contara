#pragma once

#include <algorithm>
#include <deque>
#include <iterator>
#include <list>
#include <memory>
#include <stdexcept>
#include <type_traits>
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

template<class S>
void grow_for_append(S& a, typename S::size_type extra) {
    if constexpr (requires { a.capacity(); a.reserve(extra); }) {
        const auto required = a.size() + extra;
        if (required > a.capacity()) {
            const auto capacity = a.capacity();
            const auto doubled = capacity <= a.max_size() / 2 ? capacity * 2 : a.max_size();
            a.reserve(std::max(required, doubled));
        }
    }
}

template<class S>
S& append_self(S& a) {
    const auto count = a.size();
    grow_for_append(a, count);
    if constexpr (requires { a[count]; }) {
        for (typename S::size_type i = 0; i < count; ++i) a.push_back(a[i]);
    } else {
        auto it = a.begin();
        for (typename S::size_type i = 0; i < count; ++i, ++it) a.push_back(*it);
    }
    return a;
}
} // namespace detail

template<class S> concept sequence_container = detail::sequence<S>;

template<sequence_container S>
S& append(S& a, const S& b) {
    detail::check_append_size(a, b);
    if (std::addressof(a) == std::addressof(b)) {
        return detail::append_self(a);
    }
    detail::grow_for_append(a, b.size());
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

template<sequence_container S>
S& append(S& a, S&& b) {
    detail::check_append_size(a, b);
    if (std::addressof(a) == std::addressof(b)) {
        if constexpr (std::is_copy_constructible_v<typename S::value_type>) return detail::append_self(a);
        else throw std::invalid_argument("contara: self-append requires copyable elements");
    }
    if constexpr (requires { a.splice(a.end(), b); }) {
        if (a.get_allocator() == b.get_allocator()) {
            a.splice(a.end(), b);
            return a;
        }
    }
    detail::grow_for_append(a, b.size());
    a.insert(a.end(), std::make_move_iterator(b.begin()), std::make_move_iterator(b.end()));
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

template<sequence_container S>
[[nodiscard]] S concat(const S& a, S&& b) {
    detail::check_append_size(a, b);
    S result(a.get_allocator());
    if constexpr (requires { result.reserve(a.size() + b.size()); }) result.reserve(a.size() + b.size());
    result.insert(result.end(), a.begin(), a.end());
    append(result, std::move(b));
    return result;
}

template<sequence_container S>
[[nodiscard]] S concat(S&& a, const S& b) {
    append(a, b);
    return std::move(a);
}

template<sequence_container S>
[[nodiscard]] S concat(S&& a, S&& b) {
    append(a, std::move(b));
    return std::move(a);
}

namespace ops {
template<sequence_container S>
[[nodiscard]] S operator+(const S& a, const S& b) { return concat(a, b); }
template<sequence_container S>
[[nodiscard]] S operator+(S&& a, const S& b) { return concat(std::move(a), b); }
template<sequence_container S>
[[nodiscard]] S operator+(const S& a, S&& b) { return concat(a, std::move(b)); }
template<sequence_container S>
[[nodiscard]] S operator+(S&& a, S&& b) { return concat(std::move(a), std::move(b)); }

template<sequence_container S>
S& operator+=(S& a, const S& b) { return append(a, b); }
template<sequence_container S>
S& operator+=(S& a, S&& b) { return append(a, std::move(b)); }
} // namespace ops
} // namespace contara
