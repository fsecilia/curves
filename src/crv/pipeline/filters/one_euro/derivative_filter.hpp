// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/fma.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/rounding_mode.hpp>
#include <crv/math/shifter.hpp>
#include <cassert>
#include <utility>

namespace crv::pipeline::filters::one_euro {

namespace derivative_filter_detail {

using rne_shifter_t = shifter_t<rounding_modes::shr::nearest_even>;

} // namespace derivative_filter_detail

/// Low-pass filter for the signal derivative.
///
/// The maintained 1-Euro reference implementation uses the previous filtered signal as the derivative baseline:
///
///     raw_derivative = (input - previous_filtered_input)/dt
///
/// This intentionally differs from the original 2012 paper, which uses the previous raw input.
///
/// With:
///
///     alpha_d = derivative_cutoff_rate*dt/(1 + derivative_cutoff_rate*dt)
///
/// the ordinary recurrence:
///
///     filtered_derivative
///         = previous_filtered_derivative
///         + alpha_d*(raw_derivative - previous_filtered_derivative)
///
/// rearranges to:
///
///     filtered_derivative
///         = (previous_filtered_derivative
///            + derivative_cutoff_rate*(input - previous_filtered_input))
///           /(1 + derivative_cutoff_rate*dt)
///
/// eliminating the raw-derivative division and narrowing only once at the final division.
///
/// The cutoff-interval calculator supplies the quantized `derivative_cutoff_rate*dt` representation used by the
/// denominator. An empty interval represents its mathematical limit, where alpha_d -> 1 and the filtered derivative is
/// the current raw derivative.
///
/// `x_t` uses signed storage, but this filter's signal is a nonnegative velocity magnitude. Because both signal values
/// lie in x_t's nonnegative range, `input - previous_filtered_input` is representable directly in x_t.
///
/// \pre input >= 0
/// \pre previous_filtered_input >= 0
/// \pre derivative_cutoff_rate > 0
/// \pre dt_ns > 0
template <is_fixed t_x_t, is_fixed t_dx_t, is_fixed t_cutoff_rate_t, typename t_cutoff_interval_calculator_t>
    requires(is_signed_v<t_x_t> && is_signed_v<t_dx_t> && is_signed_v<t_cutoff_rate_t>)
class derivative_filter_t
{
public:
    using x_t = t_x_t;
    using dx_t = t_dx_t;
    using cutoff_rate_t = t_cutoff_rate_t;
    using cutoff_interval_calculator_t = t_cutoff_interval_calculator_t;
    using cutoff_interval_t = typename cutoff_interval_calculator_t::cutoff_interval_t;

    using numerator_t = fixed::product_t<cutoff_rate_t, x_t>;
    using numerator_fma_t = fma_t<numerator_t, cutoff_rate_t, x_t, dx_t, derivative_filter_detail::rne_shifter_t,
        fixed::overflow_policy_t::saturate>;

    // dt_ns == 1 can produce a raw derivative spanning x_t's full signal magnitude. The divider independently
    // requires enough derivative fractional precision to avoid a left scaling shift; with equal-width signed storage,
    // the two constraints force x_t and dx_t to the same Q format.
    static_assert(
        dx_t::int_bits >= x_t::int_bits, "derivative state must contain maximum raw derivative at dt_ns == 1");
    static_assert(numerator_t::int_bits >= dx_t::int_bits, "derivative numerator must contain derivative state range");
    static_assert(
        numerator_t::frac_bits >= dx_t::frac_bits, "derivative numerator must contain derivative state precision");

    // The FMA output representation has enough headroom for every representable operand combination. Saturation is a
    // defensive policy only; it must never be reachable.
    static_assert(!numerator_fma_t::lower_saturation_possible, "derivative numerator FMA can saturate");
    static_assert(!numerator_fma_t::upper_saturation_possible, "derivative numerator FMA can saturate");

    constexpr derivative_filter_t() noexcept = default;
    constexpr explicit derivative_filter_t(
        dx_t initial, cutoff_interval_calculator_t cutoff_interval_calculator = {}) noexcept
        : cutoff_interval_calculator_{std::move(cutoff_interval_calculator)}, output_{initial}
    {}

    constexpr void reset(dx_t initial = {}) noexcept { output_ = initial; }

    template <is_fixed dt_ns_t>
        requires(!is_signed_v<dt_ns_t>)
    constexpr auto operator()(
        x_t input, x_t previous_filtered_input, cutoff_rate_t derivative_cutoff_rate, dt_ns_t dt_ns) noexcept -> dx_t
    {
        assert(input >= x_t{});
        assert(previous_filtered_input >= x_t{});
        assert(derivative_cutoff_rate > cutoff_rate_t{});
        assert(dt_ns > dt_ns_t{});

        auto const delta = input - previous_filtered_input;
        auto const interval = cutoff_interval_calculator_.calc(derivative_cutoff_rate, dt_ns);

        if (!interval)
        {
            // alpha_d -> 1, so the derivative state is the current raw derivative.
            output_ = divide<dx_t>(delta, dt_ns, rounding_modes::div::nearest_even);
            return output_;
        }

        auto const numerator = numerator_fma_t{}(derivative_cutoff_rate, delta, output_);
        auto const denominator = *interval + cutoff_interval_t{1};

        output_ = divide<dx_t>(numerator, denominator, rounding_modes::div::nearest_even);
        return output_;
    }

private:
    [[no_unique_address]] cutoff_interval_calculator_t cutoff_interval_calculator_{};
    dx_t output_{};
};

} // namespace crv::pipeline::filters::one_euro
