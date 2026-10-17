#pragma once

#include <algorithm>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <exception>
#include <functional>
#include <mutex>
#include <optional>
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
         std::movable<std::remove_cvref_t<std::invoke_result_t<F&, std::ranges::range_reference_t<const R>>>>
[[nodiscard]] auto parallel_map(const R& input, F function, parallel_options options = {}) {
    using result_type = std::remove_cvref_t<std::invoke_result_t<F&, std::ranges::range_reference_t<const R>>>;
    using difference_type = std::ranges::range_difference_t<const R>;
    if (options.min_per_worker == 0) {
        throw std::invalid_argument("contara: min_per_worker must be positive");
    }
    const auto size = static_cast<std::size_t>(std::ranges::size(input));
    const auto begin = std::ranges::begin(input);
    auto workers = options.workers;
    if (workers == 0) workers = std::max(1u, std::thread::hardware_concurrency());
    workers = std::min(workers, std::max(std::size_t{1}, size / options.min_per_worker));

    std::vector<result_type> output;
    if (workers == 1 || size == 0) {
        output.reserve(size);
        for (auto&& value : input) output.emplace_back(std::invoke(function, value));
        return output;
    }

    // Distinct optional objects also make bool results safe (unlike vector<bool> writes).
    std::vector<std::optional<result_type>> slots(size);
    std::exception_ptr error;
    std::mutex error_mutex;
    std::atomic<bool> cancelled{false};
    {
        std::vector<std::jthread> threads;
        threads.reserve(workers);
        const auto chunk = size / workers;
        const auto extra = size % workers;
        std::size_t first = 0;
        for (std::size_t worker = 0; worker < workers; ++worker) {
            const auto last = first + chunk + (worker < extra ? 1 : 0);
            threads.emplace_back([&, first, last, fn = function]() mutable {
                try {
                    for (auto i = first; i < last && !cancelled.load(std::memory_order_relaxed); ++i) {
                        slots[i].emplace(std::invoke(fn, begin[static_cast<difference_type>(i)]));
                    }
                } catch (...) {
                    std::lock_guard lock(error_mutex);
                    if (!error) error = std::current_exception();
                    cancelled.store(true, std::memory_order_relaxed);
                }
            });
            first = last;
        }
    } // jthreads join on normal return and during thread-creation failures
    if (error) std::rethrow_exception(error);
    output.reserve(size);
    for (auto& slot : slots) output.emplace_back(std::move(*slot));
    return output;
}

} // namespace contara
