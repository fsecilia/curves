// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/bitwise_enum.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/limits.hpp>
#include <crv/pipeline/filters/one_euro/signal_cutoff_rate.hpp>
#include <expected>

namespace crv::pipeline::filters::one_euro {

/// Violations that prevent a parameter set from being applied to a 1-Euro filter.
///
/// Multiple independent violations may be reported by one validation pass. The signal-cutoff overflow check is only
/// meaningful when its minimum cutoff and slope inputs are themselves valid.
enum class validation_errors_t : uint8_t
{
    none = 0,
    derivative_cutoff_rate_not_positive = 1 << 0,
    minimum_cutoff_rate_not_positive = 1 << 1,
    cutoff_slope_negative = 1 << 2,
    signal_cutoff_rate_overflow = 1 << 3,
};

} // namespace crv::pipeline::filters::one_euro

namespace crv {

template <> inline constexpr auto bitwise_for_enum_enabled<pipeline::filters::one_euro::validation_errors_t> = true;

} // namespace crv

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
///
/// Parameter objects are plain data and may temporarily contain invalid values, for example while a user edits them or
/// when they arrive through external storage such as an ioctl payload. `validate()` determines whether the complete set
/// is safe to apply.
template <is_fixed t_cutoff_rate_t, is_fixed t_cutoff_slope_t>
    requires(is_signed_v<t_cutoff_rate_t> && is_signed_v<t_cutoff_slope_t>)
struct params_t
{
    using cutoff_rate_t = t_cutoff_rate_t;
    using cutoff_slope_t = t_cutoff_slope_t;

    cutoff_rate_t derivative_cutoff_rate;
    cutoff_rate_t minimum_cutoff_rate;
    cutoff_slope_t cutoff_slope;

    /// Validates whether these parameters are safe to apply over every representable derivative state.
    ///
    /// The adaptive-cutoff check covers:
    ///
    ///     [min<dx_t>(), max<dx_t>()]
    ///
    /// and therefore uses `min<dx_t>()`, whose two's-complement magnitude is the larger endpoint.
    ///
    /// All independent violations are accumulated. The signal-cutoff overflow check is skipped when its minimum-cutoff
    /// or slope preconditions are invalid.
    template <is_fixed dx_t,
        typename signal_cutoff_rate_calculator_t = one_euro::signal_cutoff_rate_calculator_t<cutoff_rate_t>>
        requires(is_signed_v<dx_t>)
    [[nodiscard]] constexpr auto validate(signal_cutoff_rate_calculator_t const& signal_cutoff_rate_calculator
        = {}) const noexcept -> std::expected<void, validation_errors_t>
    {
        auto errors = validation_errors_t::none;

        if (derivative_cutoff_rate <= cutoff_rate_t{})
        {
            errors |= validation_errors_t::derivative_cutoff_rate_not_positive;
        }

        auto const minimum_cutoff_rate_valid = minimum_cutoff_rate > cutoff_rate_t{};
        if (!minimum_cutoff_rate_valid) errors |= validation_errors_t::minimum_cutoff_rate_not_positive;

        auto const cutoff_slope_valid = cutoff_slope >= cutoff_slope_t{};
        if (!cutoff_slope_valid) errors |= validation_errors_t::cutoff_slope_negative;

        if (minimum_cutoff_rate_valid && cutoff_slope_valid
            && !signal_cutoff_rate_calculator.try_calc(minimum_cutoff_rate, cutoff_slope, min<dx_t>()))
        {
            errors |= validation_errors_t::signal_cutoff_rate_overflow;
        }

        if (errors != validation_errors_t::none) return std::unexpected{errors};
        return {};
    }
};

} // namespace crv::pipeline::filters::one_euro
