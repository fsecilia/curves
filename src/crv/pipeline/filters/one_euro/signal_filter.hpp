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

namespace signal_filter_detail {

using rne_shifter_t = shifter_t<rounding_modes::shr::nearest_even>;

} // namespace signal_filter_detail

/// low-pass filter for input signal
///
/// The ordinary recurrence:
///
///     alpha = cutoff_rate*dt/(1 + cutoff_rate*dt)
///     filtered = previous_filtered + alpha*(input - previous_filtered)
///
/// rearranges to:
///
///     filtered = (previous_filtered + cutoff_rate*dt*input)/(1 + cutoff_rate*dt)
///
/// The cutoff-interval calculator supplies the same quantized finite `cutoff_rate*dt` representation for both the
/// numerator and denominator. An empty interval represents its mathematical limit, where alpha -> 1 and the filtered
/// signal becomes the current input exactly.
///
/// \pre input >= 0
/// \pre cutoff_rate > 0
/// \pre dt_ns > 0
template <is_fixed t_x_t, is_fixed t_cutoff_rate_t, typename t_cutoff_interval_calculator_t>
    requires(is_signed_v<t_x_t> && is_signed_v<t_cutoff_rate_t>)
class signal_filter_t
{
public:
    using x_t = t_x_t;
    using cutoff_rate_t = t_cutoff_rate_t;
    using cutoff_interval_calculator_t = t_cutoff_interval_calculator_t;
    using cutoff_interval_t = typename cutoff_interval_calculator_t::cutoff_interval_t;

    using numerator_t = fixed::product_t<cutoff_interval_t, x_t>;
    using numerator_fma_t = fma_t<numerator_t, cutoff_interval_t, x_t, x_t, signal_filter_detail::rne_shifter_t,
        fixed::overflow_policy_t::saturate>;

    static_assert(numerator_t::int_bits >= x_t::int_bits, "signal numerator must contain the signal state range");
    static_assert(numerator_t::frac_bits >= x_t::frac_bits, "signal numerator must contain the signal state precision");

    // The FMA output representation has enough headroom for every representable operand combination. Saturation is a
    // defensive policy only; it must never be reachable.
    static_assert(!numerator_fma_t::lower_saturation_possible, "signal numerator FMA can saturate");
    static_assert(!numerator_fma_t::upper_saturation_possible, "signal numerator FMA can saturate");

    constexpr signal_filter_t() noexcept = default;
    constexpr explicit signal_filter_t(
        x_t initial, cutoff_interval_calculator_t cutoff_interval_calculator = {}) noexcept
        : cutoff_interval_calculator_{std::move(cutoff_interval_calculator)}, output_{initial}
    {}

    constexpr auto output() const noexcept -> x_t { return output_; }
    constexpr void reset(x_t initial = {}) noexcept { output_ = initial; }

    template <is_fixed dt_ns_t>
        requires(!is_signed_v<dt_ns_t>)
    constexpr auto operator()(x_t input, cutoff_rate_t cutoff_rate, dt_ns_t dt_ns) noexcept -> x_t
    {
        assert(input >= x_t{});
        assert(output_ >= x_t{});
        assert(cutoff_rate > cutoff_rate_t{});
        assert(dt_ns > dt_ns_t{});

        auto const interval = cutoff_interval_calculator_.calc(cutoff_rate, dt_ns);

        if (!interval)
        {
            // alpha -> 1, so the signal state is the current input.
            output_ = input;
            return output_;
        }

        auto const numerator = numerator_fma_t{}(*interval, input, output_);
        auto const denominator = *interval + cutoff_interval_t{1};

        output_ = divide<x_t>(numerator, denominator, rounding_modes::div::nearest_even);
        return output_;
    }

private:
    [[no_unique_address]] cutoff_interval_calculator_t cutoff_interval_calculator_{};
    x_t output_{};
};

} // namespace crv::pipeline::filters::one_euro
