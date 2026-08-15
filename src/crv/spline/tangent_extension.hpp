// SPDX-License-Identifier: MIT

/// \file
/// \brief output-space spline tangent extension
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/algorithm.hpp>
#include <crv/math/integer.hpp>
#include <crv/math/limits.hpp>
#include <crv/math/shifter.hpp>
#include <climits>
#include <numeric>

namespace crv::spline {

/// linear output-space extension of the final spline tangent
///
/// The authored-curve contract makes the induced gain nondecreasing, so the stored slope is nonnegative. A positive
/// slope clamps at y_limit; a quantized zero slope continues constantly. x_max_delta is derived from this same stored
/// slope/intercept and the same rounding rule used by operator(), so the separately stored clamp point cannot disagree
/// with the actual quantized line.
template <typename t_x_t, typename t_y_t, typename t_unpacked_field_t,
    auto shifter = shifter_t<rounding_modes::shr::fast::nearest_up>{}>
struct extended_tangent_t
{
    using x_t = t_x_t;
    using y_t = t_y_t;
    using unpacked_field_t = t_unpacked_field_t;
    using mantissa_t = unpacked_field_t::mantissa_t;
    using wide_t = widened_t<mantissa_t>;

    unpacked_field_t slope; // represented real slope is mantissa * 2^(x_frac - y_frac - shift)
    y_t y0;
    x_t x_max_delta;

    /// returns the largest nonnegative raw x delta for which the stored quantized line does not exceed y_limit
    static constexpr auto clamp_delta(unpacked_field_t slope, y_t y0, y_t y_limit) noexcept -> x_t
    {
        assert(slope.mantissa >= 0);
        assert(y0 >= y_t{0});
        assert(y0 <= y_limit);

        if (slope.mantissa == 0) return max<x_t>();
        if (y0 == y_limit) return x_t{0};

        auto const within_limit = [&](typename x_t::value_t x_raw) constexpr noexcept -> bool {
            assert(x_raw >= 0);
            auto const product = widen(slope.mantissa) * x_raw;

            if (slope.shift >= 0)
            {
                assert(slope.shift < int_cast<int_t>(sizeof(wide_t) * CHAR_BIT));
                auto const delta = shifter.template shr<wide_t>(product, slope.shift);
                return widen(y0.value) + delta <= widen(y_limit.value);
            }

            // A negative stored shift is an exact left shift at runtime. Compare the unshifted nonnegative product
            // against the available output-space headroom first so this construction-side check never needs an
            // overflowing wide shift.
            auto const left_shift = -slope.shift;
            auto const headroom = widen(y_limit.value) - widen(y0.value);
            if (product == 0) return true;
            if (left_shift >= int_cast<int_t>(sizeof(wide_t) * CHAR_BIT)) return false;
            return product <= (headroom >> left_shift);
        };

        auto const x_inf_raw = max<x_t>().value;
        if (within_limit(x_inf_raw)) return max<x_t>();

        auto low = typename x_t::value_t{0};
        auto high = x_inf_raw;
        while (low + 1 < high)
        {
            auto const midpoint = std::midpoint(low, high);
            if (within_limit(midpoint)) low = midpoint;
            else high = midpoint;
        }
        return x_t::literal(low);
    }

    // \param x position relative to end of spline domain
    constexpr auto operator()(x_t x) const noexcept -> y_t
    {
        assert(slope.mantissa >= 0);
        assert(x >= x_t{0});

        auto const x_bounded = min(x.value, x_max_delta.value);
        auto const wide_product = widen(slope.mantissa) * x_bounded;

        typename y_t::value_t delta;
        if (slope.shift >= 0) delta = shifter.template shr<typename y_t::value_t>(wide_product, slope.shift);
        else
        {
            // Use an unsigned shift so malformed data cannot turn an oversized left shift into signed-overflow UB.
            // For a valid constructed tangent, clamp_delta guarantees the bounded result fits output space.
            auto const left_shift = -slope.shift;
            assert(left_shift < int_cast<int_t>(sizeof(wide_t) * CHAR_BIT));
            using unsigned_wide_t = make_unsigned_t<wide_t>;
            auto const shifted_product = static_cast<unsigned_wide_t>(wide_product) << left_shift;
            delta = int_cast<typename y_t::value_t>(shifted_product);
        }
        return y_t::literal(add_wrap(delta, y0.value));
    }
};

} // namespace crv::spline
