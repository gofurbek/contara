#pragma once

#include <contara/zip.hpp>

#include <cstddef>
#include <limits>
#include <stdexcept>

namespace contara {
namespace detail {
template<class R>
concept cartesian_common = std::ranges::common_range<R> ||
    (std::ranges::random_access_range<R> && std::ranges::sized_range<R>);

template<cartesian_common R>
constexpr auto cartesian_end(R& range) {
    if constexpr (std::ranges::common_range<R>) return std::ranges::end(range);
    else return std::ranges::begin(range) + checked_size<std::ranges::range_difference_t<R>>(std::ranges::size(range));
}
} // namespace detail

template<std::ranges::input_range Left, std::ranges::forward_range Right>
requires std::ranges::view<Left> && std::ranges::view<Right>
class cartesian_product_view : public std::ranges::view_interface<cartesian_product_view<Left, Right>> {
    Left left_;
    Right right_;
    template<bool Const, class T> using base = std::conditional_t<Const, const T, T>;

    template<bool Const>
    class iterator {
        using Parent = base<Const, cartesian_product_view>;
        using L = base<Const, Left>;
        using R = base<Const, Right>;
        using I = std::ranges::iterator_t<L>;
        using J = std::ranges::iterator_t<R>;
        Parent* parent_ = nullptr;
        I left_;
        J right_;
        bool empty_right_ = false;
        static constexpr bool random = std::ranges::random_access_range<L> &&
            std::ranges::random_access_range<R> && std::ranges::sized_range<R>;
        static constexpr bool bidirectional = std::ranges::bidirectional_range<L> &&
            std::ranges::bidirectional_range<R> && detail::cartesian_common<R>;
        static constexpr bool forward = std::ranges::forward_range<L>;
    public:
        using value_type = zip_pair<std::iter_value_t<I>, std::iter_value_t<J>>;
        using difference_type = std::common_type_t<std::iter_difference_t<I>, std::iter_difference_t<J>, std::ptrdiff_t>;
        using iterator_concept = std::conditional_t<random, std::random_access_iterator_tag,
            std::conditional_t<bidirectional, std::bidirectional_iterator_tag,
            std::conditional_t<forward, std::forward_iterator_tag, std::input_iterator_tag>>>;
        using iterator_category = std::input_iterator_tag; // dereference returns a pair proxy

        iterator() = default;
        constexpr iterator(Parent* parent, I left, J right, bool empty_right)
            : parent_(parent), left_(std::move(left)), right_(std::move(right)), empty_right_(empty_right) {}
        constexpr auto operator*() const { return zip_pair<std::iter_reference_t<I>, std::iter_reference_t<J>>(*left_, *right_); }
        constexpr iterator& operator++() {
            ++right_;
            if (right_ == std::ranges::end(parent_->right_)) {
                right_ = std::ranges::begin(parent_->right_);
                ++left_;
            }
            return *this;
        }
        constexpr void operator++(int) { ++*this; }
        constexpr iterator operator++(int) requires forward { auto old = *this; ++*this; return old; }
        constexpr iterator& operator--() requires bidirectional {
            if (right_ == std::ranges::begin(parent_->right_)) {
                --left_;
                right_ = detail::cartesian_end(parent_->right_);
            }
            --right_;
            return *this;
        }
        constexpr iterator operator--(int) requires bidirectional { auto old = *this; --*this; return old; }

    private:
        constexpr difference_type width() const requires random {
            const auto count = std::ranges::size(parent_->right_);
            using comparison = std::common_type_t<decltype(count), difference_type>;
            if (static_cast<comparison>(count) > static_cast<comparison>(std::numeric_limits<difference_type>::max()))
                throw std::length_error("contara: Cartesian row exceeds iterator difference type");
            return static_cast<difference_type>(count);
        }
        static constexpr difference_type distance(const iterator& a, const iterator& b) requires random {
            if (a.empty_right_) return 0;
            auto rows = static_cast<difference_type>(a.left_ - b.left_);
            auto columns = static_cast<difference_type>(a.right_ - b.right_);
            if (rows == 0 && columns == 0) return 0;
            const auto count = a.width();
            if (rows < 0 || (rows == 0 && columns < 0)) {
                if (columns > 0) { ++rows; columns -= count; }
                if (rows < (std::numeric_limits<difference_type>::min() - columns) / count)
                    throw std::length_error("contara: Cartesian distance exceeds iterator difference type");
                return rows * count + columns;
            }
            if (columns < 0) { --rows; columns += count; }
            if (rows > (std::numeric_limits<difference_type>::max() - columns) / count)
                throw std::length_error("contara: Cartesian distance exceeds iterator difference type");
            return rows * count + columns;
        }
    public:
        constexpr iterator& operator+=(difference_type n) requires random {
            if (n == 0) return *this;
            const auto count = width();
            if (count == 0) throw std::out_of_range("contara: advancing an empty Cartesian product");
            const auto column = static_cast<difference_type>(right_ - std::ranges::begin(parent_->right_));
            auto rows = n / count;
            const auto remainder = n % count;
            difference_type next_column;
            if (remainder >= 0) {
                if (remainder >= count - column) { ++rows; next_column = remainder - (count - column); }
                else next_column = column + remainder;
            } else {
                if (remainder < -column) { --rows; next_column = count + (column + remainder); }
                else next_column = column + remainder;
            }
            left_ += static_cast<std::iter_difference_t<I>>(rows);
            right_ = std::ranges::begin(parent_->right_) + static_cast<std::iter_difference_t<J>>(next_column);
            return *this;
        }
        constexpr iterator& operator-=(difference_type n) requires random {
            if (n < 0) { *this += -(n + 1); return *this += 1; }
            return *this += -n;
        }
        constexpr auto operator[](difference_type n) const requires random { return *(*this + n); }
        friend constexpr iterator operator+(iterator i, difference_type n) requires random { i += n; return i; }
        friend constexpr iterator operator+(difference_type n, iterator i) requires random { i += n; return i; }
        friend constexpr iterator operator-(iterator i, difference_type n) requires random { i -= n; return i; }
        friend constexpr difference_type operator-(const iterator& a, const iterator& b) requires random { return distance(a, b); }
        friend constexpr bool operator==(const iterator& a, const iterator& b) requires std::equality_comparable<I> {
            return a.empty_right_ == b.empty_right_ && (a.empty_right_ || (a.left_ == b.left_ && a.right_ == b.right_));
        }
        friend constexpr bool operator<(const iterator& a, const iterator& b) requires random {
            return !a.empty_right_ && (a.left_ < b.left_ || (a.left_ == b.left_ && a.right_ < b.right_));
        }
        friend constexpr bool operator>(const iterator& a, const iterator& b) requires random { return b < a; }
        friend constexpr bool operator<=(const iterator& a, const iterator& b) requires random { return !(b < a); }
        friend constexpr bool operator>=(const iterator& a, const iterator& b) requires random { return !(a < b); }
        constexpr bool at_end() const { return empty_right_ || left_ == std::ranges::end(parent_->left_); }
        friend constexpr bool operator==(const iterator& i, std::default_sentinel_t) { return i.at_end(); }
        friend constexpr auto iter_move(const iterator& i) {
            return zip_pair<std::iter_rvalue_reference_t<I>, std::iter_rvalue_reference_t<J>>(
                std::ranges::iter_move(i.left_), std::ranges::iter_move(i.right_));
        }
        friend constexpr void iter_swap(const iterator& a, const iterator& b)
            noexcept(noexcept(std::ranges::iter_swap(a.left_, b.left_)) && noexcept(std::ranges::iter_swap(a.right_, b.right_)))
            requires std::indirectly_swappable<I> && std::indirectly_swappable<J> {
            std::ranges::iter_swap(a.left_, b.left_);
            std::ranges::iter_swap(a.right_, b.right_);
        }
    };

    template<class L, class R>
    static constexpr auto product_size(L& left, R& right) {
        using size_type = std::common_type_t<std::ranges::range_size_t<L>, std::ranges::range_size_t<R>, std::size_t>;
        const auto n = static_cast<size_type>(std::ranges::size(left));
        const auto m = static_cast<size_type>(std::ranges::size(right));
        if (m != 0 && n > std::numeric_limits<size_type>::max() / m)
            throw std::length_error("contara: Cartesian size overflows");
        return n * m;
    }
public:
    cartesian_product_view() requires std::default_initializable<Left> && std::default_initializable<Right> = default;
    constexpr cartesian_product_view(Left left, Right right) : left_(std::move(left)), right_(std::move(right)) {}
    constexpr auto begin() {
        auto right = std::ranges::begin(right_);
        const bool empty = right == std::ranges::end(right_);
        return iterator<false>(this, std::ranges::begin(left_), std::move(right), empty);
    }
    constexpr auto end() {
        if constexpr (detail::cartesian_common<Left>) {
            auto right = std::ranges::begin(right_);
            const bool empty = right == std::ranges::end(right_);
            return iterator<false>(this, detail::cartesian_end(left_), std::move(right), empty);
        } else return std::default_sentinel;
    }
    constexpr auto begin() const requires std::ranges::input_range<const Left> && std::ranges::forward_range<const Right> {
        auto right = std::ranges::begin(right_);
        const bool empty = right == std::ranges::end(right_);
        return iterator<true>(this, std::ranges::begin(left_), std::move(right), empty);
    }
    constexpr auto end() const requires std::ranges::input_range<const Left> && std::ranges::forward_range<const Right> {
        if constexpr (detail::cartesian_common<const Left>) {
            auto right = std::ranges::begin(right_);
            const bool empty = right == std::ranges::end(right_);
            return iterator<true>(this, detail::cartesian_end(left_), std::move(right), empty);
        } else return std::default_sentinel;
    }
    constexpr auto size() requires std::ranges::sized_range<Left> && std::ranges::sized_range<Right> { return product_size(left_, right_); }
    constexpr auto size() const requires std::ranges::sized_range<const Left> && std::ranges::sized_range<const Right> { return product_size(left_, right_); }
};

template<detail::zip_source L, detail::zip_source R>
requires std::ranges::forward_range<R>
[[nodiscard]] constexpr auto cartesian_product(L&& left, R&& right) {
    auto left_view = detail::zip_all(std::forward<L>(left));
    auto right_view = detail::zip_all(std::forward<R>(right));
    return cartesian_product_view<decltype(left_view), decltype(right_view)>(std::move(left_view), std::move(right_view));
}

namespace ops {
template<detail::zip_source L, detail::zip_source R>
requires std::ranges::forward_range<R> && detail::range_operator_operands<L, R>
[[nodiscard]] constexpr auto operator*(L&& left, R&& right) {
    return contara::cartesian_product(std::forward<L>(left), std::forward<R>(right));
}
} // namespace ops
} // namespace contara
