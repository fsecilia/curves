// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/uabs.hpp>
#include <cassert>

namespace crv::pipeline::filters::one_euro {

/// Result of calculating the adaptive signal cutoff rate.
///
/// `value` is meaningful only when `overflows` is false.
template <is_fixed t_cutoff_rate_t> struct signal_cutoff_rate_result_t
{
    using cutoff_rate_t = t_cutoff_rate_t;

    cutoff_rate_t value{};
    bool overflows{};
};

/// Calculates and range-checks:
///
///     minimum_cutoff_rate + cutoff_slope*abs(filtered_derivative)
///
/// The addition is performed only after proving it cannot exceed cutoff_rate_t.
template <is_fixed cutoff_rate_t, is_fixed cutoff_slope_t, is_fixed dx_t>
    requires(is_signed_v<cutoff_rate_t> && is_signed_v<cutoff_slope_t> && is_signed_v<dx_t>)
constexpr auto try_signal_cutoff_rate(cutoff_rate_t minimum_cutoff_rate, cutoff_slope_t cutoff_slope,
    dx_t filtered_derivative) noexcept -> signal_cutoff_rate_result_t<cutoff_rate_t>
{
    assert(minimum_cutoff_rate > cutoff_rate_t{});
    assert(cutoff_slope >= cutoff_slope_t{});

    auto const adaptive_cutoff_rate = multiply(cutoff_slope, uabs(filtered_derivative));
    using accumulator_t = std::remove_cvref_t<decltype(adaptive_cutoff_rate)>;

    static_assert(is_fixed<accumulator_t>);
    static_assert(is_signed_v<accumulator_t>);
    static_assert(sizeof(typename cutoff_rate_t::value_t) < sizeof(typename accumulator_t::value_t));
    static_assert(cutoff_rate_t::frac_bits <= accumulator_t::frac_bits);
    static_assert(cutoff_rate_t::int_bits <= accumulator_t::int_bits);

    auto const minimum = accumulator_t::convert(minimum_cutoff_rate);
    auto const adaptive = accumulator_t::convert(adaptive_cutoff_rate);
    auto const maximum = accumulator_t::convert(max<cutoff_rate_t>());

    // Check before adding. This is valid because all three values are nonnegative.
    if (adaptive > maximum - minimum) return {.overflows = true};

    auto const combined = minimum + adaptive;

    return {
        .value = cutoff_rate_t::template convert<shifter_t<rounding_modes::shr::nearest_even>{}>(combined),
    };
}

template <is_fixed cutoff_rate_t, is_fixed cutoff_slope_t, is_fixed dx_t>
    requires(is_signed_v<cutoff_rate_t> && is_signed_v<cutoff_slope_t> && is_signed_v<dx_t>)
constexpr auto signal_cutoff_rate(
    cutoff_rate_t minimum_cutoff_rate, cutoff_slope_t cutoff_slope, dx_t filtered_derivative) noexcept -> cutoff_rate_t
{
    auto const result = try_signal_cutoff_rate(minimum_cutoff_rate, cutoff_slope, filtered_derivative);
    assert(!result.overflows);
    return result.value;
}

} // namespace crv::pipeline::filters::one_euro
