#pragma once

#include <contara/detail/checked_size.hpp>

#include <algorithm>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <exception>
#include <functional>
#include <mutex>
#include <ranges>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace contara {

struct parallel_options {
    std::size_t workers = 0; // 0: hardware_concurrency, with a fallback of 1
    std::size_t min_per_worker = 1024;
};

template<class R, class F>
requires std::ranges::random_access_range<const R> && std::ranges::sized_range<const R> &&
         std::copy_constructible<F> && std::invocable<F&, std::ranges::range_reference_t<const R>> &&
         std::constructible_from<std::remove_cvref_t<std::invoke_result_t<F&, std::ranges::range_reference_t<const R>>>,
                                 std::invoke_result_t<F&, std::ranges::range_reference_t<const R>>> &&
         std::move_constructible<std::remove_cvref_t<std::invoke_result_t<F&, std::ranges::range_reference_t<const R>>>>
[[nodiscard]] auto parallel_map(const R& input, F function, parallel_options options = {}) {
    using result_type = std::remove_cvref_t<std::invoke_result_t<F&, std::ranges::range_reference_t<const R>>>;
    using difference_type = std::ranges::range_difference_t<const R>;
    if (options.min_per_worker == 0) {
        throw std::invalid_argument("contara: min_per_worker must be positive");
    }
    std::vector<result_type> output;
    const auto size = detail::checked_size<std::size_t>(std::ranges::size(input), output.max_size());
    (void)detail::checked_size<difference_type>(size);
    if (size == 0) return output;
    const auto begin = std::ranges::begin(input);
    auto workers = options.workers;
    if (workers == 0) workers = std::max(1u, std::thread::hardware_concurrency());
    workers = std::min(workers, std::max(std::size_t{1}, size / options.min_per_worker));

    if (workers == 1) {
        output.reserve(size);
        for (auto&& value : input) output.emplace_back(std::invoke(function, std::forward<decltype(value)>(value)));
        return output;
    }

    const auto chunk = size / workers;
    const auto extra = size % workers;
    auto run = [&](auto consume) {
        std::exception_ptr error;
        std::mutex error_mutex;
        std::atomic<bool> cancelled{false};
        {
            std::vector<std::jthread> threads;
            threads.reserve(workers);
            std::size_t first = 0;
            try {
                for (std::size_t worker = 0; worker < workers; ++worker) {
                    const auto last = first + chunk + (worker < extra ? 1 : 0);
                    threads.emplace_back([&, first, last, worker, fn = function]() mutable {
                        try {
                            // Check cancellation per block so arithmetic loops can vectorize.
                            for (auto block = first; block < last && !cancelled.load(std::memory_order_relaxed);) {
                                const auto end = block + std::min(std::size_t{256}, last - block);
                                for (auto i = block; i < end; ++i) consume(worker, i, fn);
                                block = end;
                            }
                        } catch (...) {
                            std::lock_guard lock(error_mutex);
                            if (!error) error = std::current_exception();
                            cancelled.store(true, std::memory_order_relaxed);
                        }
                    });
                    first = last;
                }
            } catch (...) {
                cancelled.store(true, std::memory_order_relaxed);
                throw;
            }
        }
        if (error) std::rethrow_exception(error);
    };

    constexpr bool direct = !std::same_as<result_type, bool> &&
        std::is_trivially_default_constructible_v<result_type> &&
        std::is_trivially_copyable_v<result_type> &&
        std::is_assignable_v<result_type&, std::invoke_result_t<F&, std::ranges::range_reference_t<const R>>>;
    if constexpr (direct) {
        output.resize(size);
        run([&](std::size_t, std::size_t i, F& fn) {
            output[i] = std::invoke(fn, begin[static_cast<difference_type>(i)]);
        });
    } else {
        // Separate chunks avoid vector<bool> bit races and per-element optional flags.
        std::vector<std::vector<result_type>> chunks(workers);
        for (std::size_t worker = 0; worker < workers; ++worker) {
            chunks[worker].reserve(chunk + (worker < extra ? 1 : 0));
        }
        run([&](std::size_t worker, std::size_t i, F& fn) {
            chunks[worker].emplace_back(std::invoke(fn, begin[static_cast<difference_type>(i)]));
        });
        output.reserve(size);
        for (auto& values : chunks) {
            for (auto&& value : values) output.emplace_back(std::move(value));
        }
    }
    return output;
}

} // namespace contara
