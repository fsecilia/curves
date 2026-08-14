// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/uabs.hpp>
#include <cassert>
#include <optional>

namespace crv::pipeline::filters::one_euro {

template <typename t_cutoff_rate_t>
    requires(is_signed_v<t_cutoff_rate_t>)
struct cutoff_rate_calculator_t
{
    using cutoff_rate_t = t_cutoff_rate_t;

    /// Calculates and range-checks:
    ///
    ///     minimum_cutoff_rate + cutoff_slope*abs(filtered_derivative)
    ///
    /// The addition is performed only after proving it cannot exceed cutoff_rate_t.
    template <is_fixed cutoff_slope_t, is_fixed dx_t>
        requires(is_signed_v<cutoff_slope_t> && is_signed_v<dx_t>)
    constexpr auto try_calc(cutoff_rate_t minimum_cutoff_rate, cutoff_slope_t cutoff_slope,
        dx_t filtered_derivative) const noexcept -> std::optional<cutoff_rate_t>
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
        if (adaptive > maximum - minimum) return std::nullopt;

        auto const combined = minimum + adaptive;

        return cutoff_rate_t::template convert<shifter_t<rounding_modes::shr::nearest_even>{}>(combined);
    }

    template <is_fixed cutoff_slope_t, is_fixed dx_t>
        requires(is_signed_v<cutoff_slope_t> && is_signed_v<dx_t>)
    constexpr auto calc(cutoff_rate_t minimum_cutoff_rate, cutoff_slope_t cutoff_slope,
        dx_t filtered_derivative) const noexcept -> cutoff_rate_t
    {
        auto const result = try_calc(minimum_cutoff_rate, cutoff_slope, filtered_derivative);
        assert(result);
        return *result;
    }
};

} // namespace crv::pipeline::filters::one_euro
