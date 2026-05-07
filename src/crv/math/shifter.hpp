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

namespace crv {

/// shifts using rounding mode; range checks via assert
template <auto rounding_mode = rounding_modes::shr::nearest_even> struct shifter_t
{
    template <typename value_t> constexpr auto shr(value_t value, int_t count) const noexcept -> value_t
    {
        return shr_impl<value_t, value_t>(value, count);
    }

    template <int_t count, typename value_t> constexpr auto shr(value_t value) const noexcept -> value_t
    {
        return shr_impl<value_t, count, value_t>(value);
    }

    template <typename dst_t, typename src_t>
        requires(!std::same_as<dst_t, src_t>)
    constexpr auto shr(src_t src, int_t count) const noexcept -> dst_t
    {
        return shr_impl<dst_t, src_t>(src, count);
    }

    template <typename dst_t, int_t count, typename src_t>
        requires(!std::same_as<dst_t, src_t>)
    constexpr auto shr(src_t src) const noexcept -> dst_t
    {
        return shr_impl<dst_t, count, src_t>(src);
    }

    template <typename value_t> constexpr auto shl(value_t value, int_t count) const noexcept -> value_t
    {
        return shl_impl<value_t, value_t>(value, count);
    }

    template <int_t count, typename value_t> constexpr auto shl(value_t value) const noexcept -> value_t
    {
        return shl_impl<value_t, count, value_t>(value);
    }

    template <typename dst_t, typename src_t>
        requires(!std::same_as<dst_t, src_t>)
    constexpr auto shl(src_t src, int_t count) const noexcept -> dst_t
    {
        return shl_impl<dst_t, src_t>(src, count);
    }

    template <typename dst_t, int_t count, typename src_t>
        requires(!std::same_as<dst_t, src_t>)
    constexpr auto shl(src_t src) const noexcept -> dst_t
    {
        return shl_impl<dst_t, count, src_t>(src);
    }

    template <typename value_t> constexpr auto shift(value_t value, int_t count) const noexcept -> value_t
    {
        return shift_impl<value_t, value_t>(value, count);
    }

    template <int_t count, typename value_t> constexpr auto shift(value_t value) const noexcept -> value_t
    {
        return shift_impl<value_t, count, value_t>(value);
    }

    template <typename dst_t, typename src_t>
        requires(!std::same_as<dst_t, src_t>)
    constexpr auto shift(src_t src, int_t count) const noexcept -> dst_t
    {
        return shift_impl<dst_t, src_t>(src, count);
    }

    template <typename dst_t, int_t count, typename src_t>
        requires(!std::same_as<dst_t, src_t>)
    constexpr auto shift(src_t value) const noexcept -> dst_t
    {
        return shift_impl<dst_t, count, src_t>(value);
    }

private:
    template <typename dst_t, typename src_t> constexpr auto shr_impl(src_t src, int_t count) const noexcept -> dst_t
    {
        [[maybe_unused]] constexpr auto src_bits = int_cast<int_t>(sizeof(src_t) * CHAR_BIT);

        assert(count > 0 && "shifter_t: shr count must be positive");
        assert(count < src_bits && "shifter_t: shr count must be less than bit width");

        auto const unshifted = rounding_mode.bias(src, count);
        auto const shifted = int_cast<src_t>(unshifted >> count);
        return int_cast<dst_t>(rounding_mode.carry(shifted, unshifted, count));
    }

    template <typename dst_t, int_t count, typename src_t> constexpr auto shr_impl(src_t src) const noexcept -> dst_t
    {
        [[maybe_unused]] constexpr auto src_bits = int_cast<int_t>(sizeof(src_t) * CHAR_BIT);

        static_assert(count > 0, "shifter_t: shr count must be positive");
        static_assert(count < src_bits, "shifter_t: shr count must be less than bit width");

        auto const unshifted = rounding_mode.bias(src, count);
        auto const shifted = int_cast<src_t>(unshifted >> count);
        return int_cast<dst_t>(rounding_mode.carry(shifted, unshifted, count));
    }

    template <typename dst_t, typename src_t> constexpr auto shl_impl(src_t src, int_t count) const noexcept -> dst_t
    {
        [[maybe_unused]] constexpr auto dst_bits = int_cast<int_t>(sizeof(dst_t) * CHAR_BIT);

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
            assert((!cmp_greater(src, max<dst_t>() >> count)) && "shifter_t: shl positive overflow");
        }

        return int_cast<dst_t>(int_cast<dst_t>(src) << count);
    }

    template <typename dst_t, int_t count, typename src_t> constexpr auto shl_impl(src_t src) const noexcept -> dst_t
    {
        [[maybe_unused]] constexpr auto dst_bits = int_cast<int_t>(sizeof(dst_t) * CHAR_BIT);

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
            assert((!cmp_greater(src, max<dst_t>() >> count)) && "shifter_t: shl positive overflow");
        }

        return int_cast<dst_t>(int_cast<dst_t>(src) << count);
    }

    template <typename dst_t, typename src_t> constexpr auto shift_impl(src_t src, int_t count) const noexcept -> dst_t
    {
        return count >= 0 ? shl_impl<dst_t, src_t>(src, count) : shr_impl<dst_t, src_t>(src, -count);
    }

    template <typename dst_t, int_t count, typename src_t>
    constexpr auto shift_impl(src_t value) const noexcept -> dst_t
    {
        if constexpr (count >= 0) return shl_impl<dst_t, count>(value);
        else return shr_impl<dst_t, -count, src_t>(value);
    }
};

} // namespace crv
