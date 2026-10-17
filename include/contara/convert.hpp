#pragma once

#include <deque>
#include <list>
#include <ranges>
#include <set>
#include <unordered_set>
#include <utility>
#include <vector>

namespace contara {

// Copies ordinary range elements. A range of move iterators explicitly moves.
template<template<class...> class Container, std::ranges::input_range R>
[[nodiscard]] auto to(R&& range) {
    Container<std::ranges::range_value_t<R>> result;
    if constexpr (std::ranges::sized_range<R> && requires { result.reserve(std::ranges::size(range)); }) {
        result.reserve(std::ranges::size(range));
    }
    for (auto&& value : range) {
        if constexpr (requires { result.emplace_back(std::forward<decltype(value)>(value)); }) {
            result.emplace_back(std::forward<decltype(value)>(value));
        } else {
            result.emplace(std::forward<decltype(value)>(value));
        }
    }
    return result;
}

template<std::ranges::input_range R>
[[nodiscard]] auto to_vector(R&& r) { return to<std::vector>(std::forward<R>(r)); }
template<std::ranges::input_range R>
[[nodiscard]] auto to_set(R&& r) { return to<std::set>(std::forward<R>(r)); }
template<std::ranges::input_range R>
[[nodiscard]] auto to_unordered_set(R&& r) { return to<std::unordered_set>(std::forward<R>(r)); }
template<std::ranges::input_range R>
[[nodiscard]] auto to_deque(R&& r) { return to<std::deque>(std::forward<R>(r)); }
template<std::ranges::input_range R>
[[nodiscard]] auto to_list(R&& r) { return to<std::list>(std::forward<R>(r)); }

} // namespace contara
