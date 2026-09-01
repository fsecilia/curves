// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/integer.hpp>
#include <bit>
#include <cassert>
#include <climits>
#include <cmath>
#include <concepts>
#include <expected>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>

namespace crv {
namespace detail::inverse {

/// IEEE binary interchange formats supported by representable-order search
template <typename scalar_t>
concept is_supported_float = std::floating_point<scalar_t> && CHAR_BIT == 8 && std::numeric_limits<scalar_t>::is_iec559
    && std::numeric_limits<scalar_t>::radix == 2
    && ((sizeof(scalar_t) == 4 && std::numeric_limits<scalar_t>::digits == 24
            && std::numeric_limits<scalar_t>::max_exponent == 128)
        || (sizeof(scalar_t) == 8 && std::numeric_limits<scalar_t>::digits == 53
            && std::numeric_limits<scalar_t>::max_exponent == 1024)
        || (sizeof(scalar_t) == 16 && std::numeric_limits<scalar_t>::digits == 113
            && std::numeric_limits<scalar_t>::max_exponent == 16384));

template <is_supported_float scalar_t> using key_t = int_by_bytes_t<sizeof(scalar_t), false>;

template <is_supported_float scalar_t> [[nodiscard]] constexpr auto sign_mask() noexcept -> key_t<scalar_t>
{
    return key_t<scalar_t>{1} << (sizeof(scalar_t) * CHAR_BIT - 1);
}

/// monotone integer key for finite scalar order, with both zero encodings collapsed to one point
template <is_supported_float scalar_t> [[nodiscard]] constexpr auto to_key(scalar_t value) noexcept -> key_t<scalar_t>
{
    assert(std::isfinite(value) && "representable-order key requires finite input");
    auto const sign = sign_mask<scalar_t>();
    if (value == scalar_t{0}) return sign - key_t<scalar_t>{1};

    auto const bits = std::bit_cast<key_t<scalar_t>>(value);
    auto const ordered = (bits & sign) != 0 ? ~bits : bits | sign;
    return ordered >= sign ? ordered - key_t<scalar_t>{1} : ordered;
}

template <is_supported_float scalar_t> [[nodiscard]] constexpr auto from_key(key_t<scalar_t> key) noexcept -> scalar_t
{
    auto const sign = sign_mask<scalar_t>();
    auto const zero_key = sign - key_t<scalar_t>{1};
    if (key == zero_key) return scalar_t{0};

    auto const ordered = key < zero_key ? key : key + key_t<scalar_t>{1};
    auto const bits = ordered < sign ? ~ordered : ordered & ~sign;
    auto const value = std::bit_cast<scalar_t>(bits);
    assert(std::isfinite(value) && "representable-order key escaped finite interval");
    return value;
}

template <typename result_t>
concept is_expected_bool = requires {
    typename result_t::value_type;
    typename result_t::error_type;
} && std::same_as<typename result_t::value_type, bool>;

} // namespace detail::inverse

/// error-propagating first-true search in finite representable scalar order
struct try_bisect_first_true_t
{
    /// finds the first representable x in [low, high] whose monotone predicate is true
    template <detail::inverse::is_supported_float scalar_t, typename predicate_t>
        requires detail::inverse::is_expected_bool<std::invoke_result_t<predicate_t const&, scalar_t>>
    [[nodiscard]] constexpr auto operator()(scalar_t low, scalar_t high, predicate_t const& predicate) const noexcept
    {
        using predicate_result_t = std::invoke_result_t<predicate_t const&, scalar_t>;
        using error_t = predicate_result_t::error_type;
        using result_t = std::expected<std::optional<scalar_t>, error_t>;

        assert(std::isfinite(low) && std::isfinite(high) && "representable-order search requires finite endpoints");
        assert(low <= high && "invalid search range");

        auto low_result = predicate(low);
        if (!low_result) return result_t{std::unexpected{std::move(low_result).error()}};
        if (*low_result) return result_t{std::optional<scalar_t>{low}};
        if (low == high) return result_t{std::nullopt};

        auto high_result = predicate(high);
        if (!high_result) return result_t{std::unexpected{std::move(high_result).error()}};
        if (!*high_result) return result_t{std::nullopt};

        auto false_key = detail::inverse::to_key(low);
        auto true_key = detail::inverse::to_key(high);
        assert(false_key < true_key && "ordered search endpoints must be distinct");

        while (true_key - false_key > 1)
        {
            auto const mid_key = false_key + (true_key - false_key) / 2;
            auto const mid = detail::inverse::from_key<scalar_t>(mid_key);
            auto mid_result = predicate(mid);
            if (!mid_result) return result_t{std::unexpected{std::move(mid_result).error()}};

            if (*mid_result) true_key = mid_key;
            else false_key = mid_key;
        }

        return result_t{std::optional<scalar_t>{detail::inverse::from_key<scalar_t>(true_key)}};
    }
};

/// first-true search in finite representable scalar order
struct bisect_first_true_t
{
    template <detail::inverse::is_supported_float scalar_t, typename predicate_t>
    [[nodiscard]] constexpr auto operator()(scalar_t low, scalar_t high, predicate_t const& predicate) const noexcept
        -> std::optional<scalar_t>
    {
        enum class no_error_t : uint8_t
        {
        };

        auto const result = try_bisect_first_true_t{}(low, high,
            [&predicate](scalar_t input) noexcept { return std::expected<bool, no_error_t>{predicate(input)}; });
        assert(result.has_value() && "non-error predicate unexpectedly failed");
        return *result;
    }
};

/// target-based lower-bound adapter over representable-order first-true search
struct bisect_lower_bound_t
{
    /// finds the leftmost x in [low, high] where f(x) >= target, if any
    template <detail::inverse::is_supported_float scalar_t, typename monotone_t>
    [[nodiscard]] constexpr auto operator()(
        scalar_t low, scalar_t high, scalar_t target, monotone_t const& f) const noexcept -> std::optional<scalar_t>
    {
        return bisect_first_true_t{}(low, high, [&f, target](scalar_t input) noexcept { return f(input) >= target; });
    }
};

} // namespace crv
