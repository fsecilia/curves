// SPDX-License-Identifier: MIT

/// \file
/// \brief combines shifting with a rounding mode
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/cmp.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/limits.hpp>
#include <crv/math/rounding_mode.hpp>
#include <climits>
#include <utility>

namespace crv {

/// shifts using rounding mode; range checks via assert
template <typename rounding_mode_t = rounding_modes::shr::nearest_even_t> class shifter_t
{
public:
    explicit constexpr shifter_t(rounding_mode_t rounding_mode = {}) noexcept : rounding_mode_{std::move(rounding_mode)}
    {}

    template <typename value_t> constexpr auto shr(value_t value, int_t count) const noexcept -> value_t
    {
        return shr<value_t, value_t>(value, count);
    }

    template <int_t count, typename value_t> constexpr auto shr(value_t value) const noexcept -> value_t
    {
        return shr<value_t, count, value_t>(value);
    }

    template <typename dst_t, typename src_t> constexpr auto shr(src_t src, int_t count) const noexcept -> dst_t
    {
        constexpr auto src_bits = int_cast<int_t>(sizeof(src_t) * CHAR_BIT);

        assert(count > 0 && "shifter_t: shr count must be positive");
        assert(count < src_bits && "shifter_t: shr count must be less than bit width");

        auto const unshifted = rounding_mode_.bias(src, count);
        auto const shifted   = int_cast<src_t>(unshifted >> count);
        return int_cast<dst_t>(rounding_mode_.carry(shifted, unshifted, count));
    }

    template <typename dst_t, int_t count, typename src_t> constexpr auto shr(src_t src) const noexcept -> dst_t
    {
        constexpr auto src_bits = int_cast<int_t>(sizeof(src_t) * CHAR_BIT);

        static_assert(count > 0, "shifter_t: shr count must be positive");
        static_assert(count < src_bits, "shifter_t: shr count must be less than bit width");

        auto const unshifted = rounding_mode_.bias(src, count);
        auto const shifted   = int_cast<src_t>(unshifted >> count);
        return int_cast<dst_t>(rounding_mode_.carry(shifted, unshifted, count));
    }

    template <typename value_t> constexpr auto shl(value_t value, int_t count) const noexcept -> value_t
    {
        return shl<value_t, value_t>(value, count);
    }

    template <int_t count, typename value_t> constexpr auto shl(value_t value) const noexcept -> value_t
    {
        return shl<value_t, count, value_t>(value);
    }

    template <typename dst_t, typename src_t> constexpr auto shl(src_t src, int_t count) const noexcept -> dst_t
    {
        constexpr auto dst_bits = int_cast<int_t>(sizeof(dst_t) * CHAR_BIT);

        assert(count >= 0 && "shifter_t: shl count must not be negative");
        assert(count < dst_bits && "shifter_t: shl count larger than target bit width");

        // check value fits after shifting without overflowing
        if constexpr (is_signed_v<src_t>)
        {
            assert((!cmp_greater(src, max<dst_t>() >> count)) && "shifter_t: shl positive overflow");
            assert((!cmp_less(src, min<dst_t>() >> count)) && "shifter_t: shl negative overflow");
        }
        else
        {
            assert((!cmp_greater(src, max<dst_t>() >> count)) && "shifter_t: shl overflow");
        }

        return int_cast<dst_t>(int_cast<dst_t>(src) << count);
    }

    template <typename dst_t, int_t count, typename src_t> constexpr auto shl(src_t src) const noexcept -> dst_t
    {
        constexpr auto dst_bits = int_cast<int_t>(sizeof(dst_t) * CHAR_BIT);

        static_assert(count >= 0, "shifter_t: shl count must not be negative");
        static_assert(count < dst_bits, "shifter_t: shl count larger than target bit width");

        // check value fits after shifting without overflowing
        if constexpr (is_signed_v<src_t>)
        {
            assert((!cmp_greater(src, max<dst_t>() >> count)) && "shifter_t: shl positive overflow");
            assert((!cmp_less(src, min<dst_t>() >> count)) && "shifter_t: shl negative overflow");
        }
        else
        {
            assert((!cmp_greater(src, max<dst_t>() >> count)) && "shifter_t: shl overflow");
        }

        return int_cast<dst_t>(int_cast<dst_t>(src) << count);
    }

    template <typename value_t> constexpr auto shift(value_t value, int_t count) const noexcept -> value_t
    {
        return shift<value_t, value_t>(value, count);
    }

    template <int_t count, typename value_t> constexpr auto shift(value_t value) const noexcept -> value_t
    {
        return shift<value_t, count, value_t>(value);
    }

    template <typename dst_t, typename src_t> constexpr auto shift(src_t src, int_t count) const noexcept -> dst_t
    {
        return count >= 0 ? shl<dst_t, src_t>(src, count) : shr<dst_t, src_t>(src, -count);
    }

    template <typename dst_t, int_t count, typename src_t> constexpr auto shift(src_t value) const noexcept -> dst_t
    {
        if constexpr (count >= 0) return shl<dst_t, count>(value);
        else return shr<dst_t, -count, src_t>(value);
    }

private:
    [[no_unique_address]] rounding_mode_t rounding_mode_{};
};

} // namespace crv
