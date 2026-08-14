// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/limits.hpp>
#include <crv/pipeline/filters/one_euro/signal_cutoff_rate.hpp>
#include <expected>

namespace crv::pipeline::filters::one_euro {

/// Preconverted runtime parameters for a 1-Euro filter.
///
/// Runtime units:
///
///     derivative_cutoff_rate  1/ns
///     minimum_cutoff_rate     1/ns
///     cutoff_slope            1/signal-unit
///
/// The filtered derivative has units signal-unit/ns, so:
///
///     minimum_cutoff_rate + cutoff_slope*abs(filtered_derivative)
///
/// has units 1/ns.
///
/// Given paper-style cutoff frequencies in Hz:
///
///     cutoff_rate = 2*pi*f*1e-9
///
/// Given paper-style beta in Hz/(signal-unit/second):
///
///     cutoff_slope = 2*pi*beta
template <is_fixed t_cutoff_rate_t, is_fixed t_cutoff_slope_t>
    requires(is_signed_v<t_cutoff_rate_t> && is_signed_v<t_cutoff_slope_t>)
struct params_t
{
    using cutoff_rate_t = t_cutoff_rate_t;
    using cutoff_slope_t = t_cutoff_slope_t;

    cutoff_rate_t derivative_cutoff_rate;
    cutoff_rate_t minimum_cutoff_rate;
    cutoff_slope_t cutoff_slope;

    enum class validation_error
    {
        derivative_cutoff_rate_not_positive,
        minimum_cutoff_rate_not_positive,
        cutoff_slope_negative,
        signal_cutoff_rate_overflow,
    };

    /// Validates whether these parameters are safe to apply over every representable derivative state.
    ///
    /// Parameter objects are plain data and may temporarily contain invalid values, for example while a user edits them
    /// or when they arrive through external storage such as an ioctl payload.
    ///
    /// The adaptive-cutoff check covers:
    ///
    ///     [min<dx_t>(), max<dx_t>()]
    ///
    /// and therefore uses `min<dx_t>()`, whose two's-complement magnitude is the larger endpoint.
    template <is_fixed dx_t,
        typename signal_cutoff_rate_calculator_t = one_euro::signal_cutoff_rate_calculator_t<cutoff_rate_t>>
        requires(is_signed_v<dx_t>)
    [[nodiscard]] constexpr auto validate(signal_cutoff_rate_calculator_t const& signal_cutoff_rate_calculator
        = {}) const noexcept -> std::expected<void, validation_error>
    {
        if (derivative_cutoff_rate <= cutoff_rate_t{})
        {
            return std::unexpected{validation_error::derivative_cutoff_rate_not_positive};
        }

        if (minimum_cutoff_rate <= cutoff_rate_t{})
        {
            return std::unexpected{validation_error::minimum_cutoff_rate_not_positive};
        }

        if (cutoff_slope < cutoff_slope_t{}) { return std::unexpected{validation_error::cutoff_slope_negative}; }

        if (!signal_cutoff_rate_calculator.try_calc(minimum_cutoff_rate, cutoff_slope, min<dx_t>()))
        {
            return std::unexpected{validation_error::signal_cutoff_rate_overflow};
        }

        return {};
    }
};

} // namespace crv::pipeline::filters::one_euro
