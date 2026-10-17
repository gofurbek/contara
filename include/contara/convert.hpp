#pragma once

#include <concepts>
#include <deque>
#include <list>
#include <ranges>
#include <set>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

namespace contara {

// Matching container rvalues transfer storage. Other ordinary ranges copy values.
template<template<class...> class Container, std::ranges::input_range R>
[[nodiscard]] auto to(R&& range) {
    using result_type = Container<std::ranges::range_value_t<R>>;
    if constexpr (std::same_as<std::remove_cvref_t<R>, result_type>) {
        return result_type(std::forward<R>(range));
    } else if constexpr (std::same_as<result_type, std::set<std::ranges::range_value_t<R>>> &&
                         std::ranges::common_range<R>) {
        // The standard range constructor is linear for already sorted input.
        return result_type(std::ranges::begin(range), std::ranges::end(range));
    } else {
        result_type result;
        if constexpr (std::ranges::sized_range<R> && requires { result.reserve(std::ranges::size(range)); }) {
            result.reserve(std::ranges::size(range));
        }
        if constexpr (std::same_as<result_type, std::set<std::ranges::range_value_t<R>>>) {
            auto previous = result.end();
            const auto comp = result.key_comp();
            for (auto&& value : range) {
                // Sorted streams stay linear, including runs of duplicate keys.
                if (previous != result.end() && !comp(value, *previous) && !comp(*previous, value)) continue;
                previous = result.emplace_hint(result.end(), std::forward<decltype(value)>(value));
            }
        } else {
            for (auto&& value : range) {
                if constexpr (requires { result.emplace_back(std::forward<decltype(value)>(value)); }) {
                    result.emplace_back(std::forward<decltype(value)>(value));
                } else {
                    result.emplace(std::forward<decltype(value)>(value));
                }
            }
        }
        return result;
    }
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
