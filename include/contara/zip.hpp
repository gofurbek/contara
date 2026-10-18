#pragma once

#include <contara/detail/checked_size.hpp>

#include <algorithm>
#include <concepts>
#include <initializer_list>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <utility>

namespace contara {

// Pair-like values/references with C++20 common-reference support, including
// move-only elements. Structured bindings and .first/.second both work.
template<class A, class B>
struct zip_pair {
    A first;
    B second;

    zip_pair() requires std::default_initializable<A> && std::default_initializable<B> = default;
    constexpr zip_pair(A a, B b) : first(std::forward<A>(a)), second(std::forward<B>(b)) {}

    template<class C, class D> requires std::convertible_to<C&, A> && std::convertible_to<D&, B>
    constexpr zip_pair(zip_pair<C, D>& other) : first(other.first), second(other.second) {}
    template<class C, class D> requires std::convertible_to<const C&, A> && std::convertible_to<const D&, B>
    constexpr zip_pair(const zip_pair<C, D>& other) : first(other.first), second(other.second) {}
    template<class C, class D> requires std::convertible_to<C&&, A> && std::convertible_to<D&&, B>
    constexpr zip_pair(zip_pair<C, D>&& other)
        : first(std::forward<C>(other.first)), second(std::forward<D>(other.second)) {}
    template<class C, class D> requires std::convertible_to<const C&&, A> && std::convertible_to<const D&&, B>
    constexpr zip_pair(const zip_pair<C, D>&& other)
        : first(std::forward<const C>(other.first)), second(std::forward<const D>(other.second)) {}
};

template<class A, class B, class C, class D>
requires requires(const zip_pair<A, B>& a, const zip_pair<C, D>& b) {
    { a.first == b.first } -> std::convertible_to<bool>;
    { a.second == b.second } -> std::convertible_to<bool>;
}
constexpr bool operator==(const zip_pair<A, B>& a, const zip_pair<C, D>& b) {
    return a.first == b.first && a.second == b.second;
}

template<class A, class B, class C, class D>
requires requires(const zip_pair<A, B>& a, const zip_pair<C, D>& b) {
    { a.first < b.first } -> std::convertible_to<bool>;
    { b.first < a.first } -> std::convertible_to<bool>;
    { a.second < b.second } -> std::convertible_to<bool>;
}
constexpr bool operator<(const zip_pair<A, B>& a, const zip_pair<C, D>& b) {
    return a.first < b.first || (!(b.first < a.first) && a.second < b.second);
}
template<class A, class B, class C, class D>
requires requires(const zip_pair<C, D>& b, const zip_pair<A, B>& a) { b < a; }
constexpr bool operator>(const zip_pair<A, B>& a, const zip_pair<C, D>& b) { return b < a; }
template<class A, class B, class C, class D>
requires requires(const zip_pair<C, D>& b, const zip_pair<A, B>& a) { b < a; }
constexpr bool operator<=(const zip_pair<A, B>& a, const zip_pair<C, D>& b) { return !(b < a); }
template<class A, class B, class C, class D>
requires requires(const zip_pair<A, B>& a, const zip_pair<C, D>& b) { a < b; }
constexpr bool operator>=(const zip_pair<A, B>& a, const zip_pair<C, D>& b) { return !(a < b); }

} // namespace contara

// A permitted standard customization for this library's own pair-like type.
// std::pair's equivalent common-reference support requires a newer language mode.
namespace std {
template<class A, class B, class C, class D, template<class> class TQ, template<class> class UQ>
requires requires { typename common_reference_t<TQ<A>, UQ<C>>; typename common_reference_t<TQ<B>, UQ<D>>; }
struct basic_common_reference<contara::zip_pair<A, B>, contara::zip_pair<C, D>, TQ, UQ> {
    using type = contara::zip_pair<common_reference_t<TQ<A>, UQ<C>>, common_reference_t<TQ<B>, UQ<D>>>;
};
} // namespace std

namespace contara {
namespace detail {

template<class> inline constexpr bool zip_initializer_list = false;
template<class T> inline constexpr bool zip_initializer_list<std::initializer_list<T>> = true;

// Compatibility for C++20 libraries predating the owning_view defect report.
template<std::ranges::input_range R> requires std::movable<R>
class owning_zip_view : public std::ranges::view_interface<owning_zip_view<R>> {
    R range_;
public:
    owning_zip_view() requires std::default_initializable<R> = default;
    explicit constexpr owning_zip_view(R&& range) : range_(std::move(range)) {}
    owning_zip_view(owning_zip_view&&) = default;
    owning_zip_view& operator=(owning_zip_view&&) = default;
    constexpr auto begin() { return std::ranges::begin(range_); }
    constexpr auto end() { return std::ranges::end(range_); }
    constexpr auto begin() const requires std::ranges::range<const R> { return std::ranges::begin(range_); }
    constexpr auto end() const requires std::ranges::range<const R> { return std::ranges::end(range_); }
    constexpr auto size() requires std::ranges::sized_range<R> { return std::ranges::size(range_); }
    constexpr auto size() const requires std::ranges::sized_range<const R> { return std::ranges::size(range_); }
};

template<class R>
concept zip_source = std::ranges::input_range<R> && (std::ranges::viewable_range<R> ||
    (!std::is_lvalue_reference_v<R> && std::movable<std::remove_reference_t<R>> &&
     !zip_initializer_list<std::remove_cvref_t<R>>));

// Input ranges are arrays or user-defined types. Two raw arrays cannot be
// operands of an overloaded binary operator; the named functions still work.
template<class L, class R>
concept range_operator_operands = !std::is_array_v<std::remove_cvref_t<L>> ||
                                 !std::is_array_v<std::remove_cvref_t<R>>;

template<zip_source R>
constexpr auto zip_all(R&& range) {
    if constexpr (std::ranges::viewable_range<R>) return std::views::all(std::forward<R>(range));
    else return owning_zip_view<std::remove_cvref_t<R>>(std::forward<R>(range));
}

} // namespace detail

template<std::ranges::input_range Left, std::ranges::input_range Right>
requires std::ranges::view<Left> && std::ranges::view<Right>
class zip_view : public std::ranges::view_interface<zip_view<Left, Right>> {
    Left left_;
    Right right_;

    template<bool Const, class V> using base = std::conditional_t<Const, const V, V>;
    template<bool> class iterator;
    template<bool Const>
    class sentinel {
        template<bool> friend class iterator;
        std::ranges::sentinel_t<base<Const, Left>> left_;
        std::ranges::sentinel_t<base<Const, Right>> right_;
    public:
        sentinel() = default;
        constexpr sentinel(decltype(left_) left, decltype(right_) right)
            : left_(std::move(left)), right_(std::move(right)) {}
    };

    template<bool Const>
    class iterator {
        using I = std::ranges::iterator_t<base<Const, Left>>;
        using J = std::ranges::iterator_t<base<Const, Right>>;
        I left_;
        J right_;
        static constexpr bool random = std::random_access_iterator<I> && std::random_access_iterator<J>;
        static constexpr bool bidirectional = std::bidirectional_iterator<I> && std::bidirectional_iterator<J>;
        static constexpr bool forward = std::forward_iterator<I> && std::forward_iterator<J>;
        static constexpr bool sized_end =
            std::sized_sentinel_for<std::ranges::sentinel_t<base<Const, Left>>, I> &&
            std::sized_sentinel_for<std::ranges::sentinel_t<base<Const, Right>>, J>;
    public:
        using value_type = zip_pair<std::iter_value_t<I>, std::iter_value_t<J>>;
        using difference_type = std::common_type_t<std::iter_difference_t<I>, std::iter_difference_t<J>>;
        using iterator_concept = std::conditional_t<random, std::random_access_iterator_tag,
            std::conditional_t<bidirectional, std::bidirectional_iterator_tag,
            std::conditional_t<forward, std::forward_iterator_tag, std::input_iterator_tag>>>;
        using iterator_category = std::input_iterator_tag; // dereference yields a pair proxy

        iterator() = default;
        constexpr iterator(I left, J right) : left_(std::move(left)), right_(std::move(right)) {}
        constexpr auto operator*() const { return zip_pair<std::iter_reference_t<I>, std::iter_reference_t<J>>(*left_, *right_); }
        constexpr iterator& operator++() { ++left_; ++right_; return *this; }
        constexpr void operator++(int) { ++*this; }
        constexpr iterator operator++(int) requires forward { auto old = *this; ++*this; return old; }
        constexpr iterator& operator--() requires bidirectional { --left_; --right_; return *this; }
        constexpr iterator operator--(int) requires bidirectional { auto old = *this; --*this; return old; }
        constexpr iterator& operator+=(difference_type n) requires random {
            left_ += static_cast<std::iter_difference_t<I>>(n);
            right_ += static_cast<std::iter_difference_t<J>>(n);
            return *this;
        }
        constexpr iterator& operator-=(difference_type n) requires random {
            left_ -= static_cast<std::iter_difference_t<I>>(n);
            right_ -= static_cast<std::iter_difference_t<J>>(n);
            return *this;
        }
        constexpr auto operator[](difference_type n) const requires random { return *(*this + n); }
        friend constexpr iterator operator+(iterator i, difference_type n) requires random { i += n; return i; }
        friend constexpr iterator operator+(difference_type n, iterator i) requires random { i += n; return i; }
        friend constexpr iterator operator-(iterator i, difference_type n) requires random { i -= n; return i; }
        friend constexpr difference_type operator-(const iterator& a, const iterator& b) requires random { return a.left_ - b.left_; }
        friend constexpr bool operator==(const iterator& a, const iterator& b)
            requires std::equality_comparable<I> && std::equality_comparable<J> {
            return a.left_ == b.left_ && a.right_ == b.right_;
        }
        friend constexpr bool operator<(const iterator& a, const iterator& b) requires random { return a.left_ < b.left_; }
        friend constexpr bool operator>(const iterator& a, const iterator& b) requires random { return b < a; }
        friend constexpr bool operator<=(const iterator& a, const iterator& b) requires random { return !(b < a); }
        friend constexpr bool operator>=(const iterator& a, const iterator& b) requires random { return !(a < b); }

        constexpr bool at_end(const sentinel<Const>& end) const { return left_ == end.left_ || right_ == end.right_; }
        constexpr difference_type remaining(const sentinel<Const>& end) const requires sized_end {
            return std::min(static_cast<difference_type>(end.left_ - left_), static_cast<difference_type>(end.right_ - right_));
        }
        friend constexpr bool operator==(const iterator& i, const sentinel<Const>& end) { return i.at_end(end); }
        friend constexpr difference_type operator-(const sentinel<Const>& end, const iterator& i) requires sized_end { return i.remaining(end); }
        friend constexpr difference_type operator-(const iterator& i, const sentinel<Const>& end) requires sized_end { return -i.remaining(end); }
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
    static constexpr auto shorter_size(L& left, R& right) {
        using size_type = std::common_type_t<std::ranges::range_size_t<L>, std::ranges::range_size_t<R>>;
        return std::min(static_cast<size_type>(std::ranges::size(left)), static_cast<size_type>(std::ranges::size(right)));
    }
public:
    zip_view() requires std::default_initializable<Left> && std::default_initializable<Right> = default;
    constexpr zip_view(Left left, Right right) : left_(std::move(left)), right_(std::move(right)) {}
    constexpr auto begin() { return iterator<false>(std::ranges::begin(left_), std::ranges::begin(right_)); }
    constexpr auto end() {
        if constexpr (std::ranges::random_access_range<Left> && std::ranges::random_access_range<Right> &&
                      std::ranges::sized_range<Left> && std::ranges::sized_range<Right>) {
            return begin() + detail::checked_size<typename iterator<false>::difference_type>(size());
        } else return sentinel<false>(std::ranges::end(left_), std::ranges::end(right_));
    }
    constexpr auto begin() const requires std::ranges::input_range<const Left> && std::ranges::input_range<const Right> {
        return iterator<true>(std::ranges::begin(left_), std::ranges::begin(right_));
    }
    constexpr auto end() const requires std::ranges::input_range<const Left> && std::ranges::input_range<const Right> {
        if constexpr (std::ranges::random_access_range<const Left> && std::ranges::random_access_range<const Right> &&
                      std::ranges::sized_range<const Left> && std::ranges::sized_range<const Right>) {
            return begin() + detail::checked_size<typename iterator<true>::difference_type>(size());
        } else return sentinel<true>(std::ranges::end(left_), std::ranges::end(right_));
    }
    constexpr auto size() requires std::ranges::sized_range<Left> && std::ranges::sized_range<Right> { return shorter_size(left_, right_); }
    constexpr auto size() const requires std::ranges::sized_range<const Left> && std::ranges::sized_range<const Right> { return shorter_size(left_, right_); }
};

template<detail::zip_source L, detail::zip_source R>
[[nodiscard]] constexpr auto zip(L&& left, R&& right) {
    auto left_view = detail::zip_all(std::forward<L>(left));
    auto right_view = detail::zip_all(std::forward<R>(right));
    return zip_view<decltype(left_view), decltype(right_view)>(std::move(left_view), std::move(right_view));
}

namespace ops {
template<detail::zip_source L, detail::zip_source R>
requires detail::range_operator_operands<L, R>
[[nodiscard]] constexpr auto operator%(L&& left, R&& right) {
    return contara::zip(std::forward<L>(left), std::forward<R>(right));
}
} // namespace ops

} // namespace contara

namespace std::ranges {
template<class L, class R>
inline constexpr bool enable_borrowed_range<contara::zip_view<L, R>> = enable_borrowed_range<L> && enable_borrowed_range<R>;
} // namespace std::ranges
