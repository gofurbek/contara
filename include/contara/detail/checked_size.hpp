#pragma once

#include <limits>
#include <stdexcept>
#include <type_traits>

namespace contara::detail {
template<class To, class From>
constexpr To checked_size(From value, To limit = std::numeric_limits<To>::max()) {
    if constexpr (std::numeric_limits<From>::is_signed) {
        if (value < 0) throw std::length_error("contara: negative range size");
    }
    using comparison = std::common_type_t<From, To>;
    if (static_cast<comparison>(value) > static_cast<comparison>(limit))
        throw std::length_error("contara: range size exceeds destination limit");
    return static_cast<To>(value);
}
} // namespace contara::detail
