// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/uabs.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/rounding_mode.hpp>
#include <crv/math/shifter.hpp>
#include <cassert>
#include <utility>

namespace crv::pipeline::filters::one_euro {

namespace detail {

static constexpr auto rne_shifter = shifter_t<rounding_modes::shr::nearest_even>{};

/// nearest-even fixed-point division
template <is_fixed output_t, is_fixed numerator_t, is_fixed denominator_t>
constexpr auto divide_rne(numerator_t numerator, denominator_t denominator) noexcept -> output_t
{
    return divide<output_t, rne_shifter>(numerator, denominator);
}

} // namespace detail

/// finite cutoff step or its positive-infinity limiting case
///
/// `saturated` means the real cutoff step exceeded the working representation. Consumers apply the mathematical
/// u -> infinity limit rather than operating on the finite placeholder value.
template <is_fixed t_cutoff_step_t>
    requires(!is_signed_v<t_cutoff_step_t>)
struct bounded_cutoff_step_t
{
    using cutoff_step_t = t_cutoff_step_t;

    cutoff_step_t value{};
    bool saturated{};
};

/// narrows a wide dimensionless cutoff-step product into its working format
///
/// The largest finite output reserves room for denominator `1 + u`. Oversized products are reported separately so
/// consumers can apply the correct limiting behavior.
///
/// Pipeline validation must prove that the smallest supported positive product does not round to zero.
template <is_fixed t_cutoff_step_t>
    requires(!is_signed_v<t_cutoff_step_t> && !is_fixed_frac<t_cutoff_step_t>)
struct cutoff_step_limiter_t
{
    using cutoff_step_t = t_cutoff_step_t;
    using output_t = bounded_cutoff_step_t<cutoff_step_t>;

    static constexpr auto one = cutoff_step_t{1};

    /// largest finite u for which 1 + u remains representable.
    static constexpr auto ceiling = max<cutoff_step_t>() - one;

    template <is_fixed cutoff_step_product_t>
        requires(!is_signed_v<cutoff_step_product_t>
            && sizeof(typename cutoff_step_t::value_t) <= sizeof(typename cutoff_step_product_t::value_t)
            && cutoff_step_t::frac_bits <= cutoff_step_product_t::frac_bits
            && cutoff_step_t::int_bits <= cutoff_step_product_t::int_bits)
    constexpr auto operator()(cutoff_step_product_t product) const noexcept -> output_t
    {
        static constexpr auto product_ceiling = cutoff_step_product_t::convert(ceiling);

        if (product > product_ceiling) return {ceiling, true};

        auto const value = cutoff_step_t::template convert<detail::rne_shifter>(product);

        assert(product == cutoff_step_product_t{} || value != cutoff_step_t{});

        return {value, false};
    }
};

/// combines a minimum cutoff rate and a wide adaptive cutoff rate
template <is_fixed t_cutoff_rate_t>
    requires(!is_signed_v<t_cutoff_rate_t>)
struct cutoff_rate_combiner_t
{
    using cutoff_rate_t = t_cutoff_rate_t;

    template <is_fixed adaptive_cutoff_rate_t>
        requires(!is_signed_v<adaptive_cutoff_rate_t>
            && sizeof(typename cutoff_rate_t::value_t) < sizeof(typename adaptive_cutoff_rate_t::value_t)
            && cutoff_rate_t::frac_bits <= adaptive_cutoff_rate_t::frac_bits
            && cutoff_rate_t::int_bits <= adaptive_cutoff_rate_t::int_bits)
    constexpr auto operator()(
        cutoff_rate_t minimum_cutoff_rate, adaptive_cutoff_rate_t adaptive_cutoff_rate) const noexcept -> cutoff_rate_t
    {
        using accumulator_t = fixed_t<typename adaptive_cutoff_rate_t::value_t, adaptive_cutoff_rate_t::frac_bits>;

        auto const adaptive = accumulator_t::convert(adaptive_cutoff_rate);
        auto const minimum = accumulator_t::convert(minimum_cutoff_rate);
        auto const output_max = accumulator_t::convert(max<cutoff_rate_t>());

        if (adaptive > output_max - minimum) return max<cutoff_rate_t>();

        /// saturate
        return cutoff_rate_t::template convert<detail::rne_shifter>(adaptive + minimum);
    }
};

/// low-pass filter for derivative estimate
template <is_fixed t_x_t, is_fixed t_dx_t, is_fixed t_cutoff_rate_t, is_fixed t_cutoff_step_t,
    typename t_cutoff_step_limiter_t = cutoff_step_limiter_t<t_cutoff_step_t>>
    requires(
        is_signed_v<t_x_t> && is_signed_v<t_dx_t> && !is_signed_v<t_cutoff_rate_t> && !is_signed_v<t_cutoff_step_t>)
class derivative_low_pass_t
{
public:
    using x_t = t_x_t;
    using dx_t = t_dx_t;
    using cutoff_rate_t = t_cutoff_rate_t;
    using cutoff_step_t = t_cutoff_step_t;
    using cutoff_step_limiter_t = t_cutoff_step_limiter_t;

    constexpr derivative_low_pass_t() noexcept = default;

    constexpr explicit derivative_low_pass_t(dx_t initial, cutoff_step_limiter_t limit_cutoff_step = {}) noexcept
        : output_{initial}, limit_cutoff_step_{std::move(limit_cutoff_step)}
    {}

    constexpr auto output() const noexcept -> dx_t { return output_; }

    constexpr void reset(dx_t initial = {}) noexcept { output_ = initial; }

    /// implements paper's derivative leg after algebraically eliminating the raw derivative division
    ///
    ///     d_raw = (x - previous_filtered_x)/dt
    ///     alpha_d = omega_d*dt/(1 + omega_d*dt)
    ///     filtered_d = filtered_d_prev + alpha_d*(d_raw - filtered_d_prev)
    ///
    /// Rearranged:
    ///
    ///     filtered_d = (filtered_d_prev + omega_d*(x - previous_filtered_x))/(1 + omega_d*dt)
    ///
    /// Runtime units:
    ///
    ///     x                   signal units
    ///     dt                  ns
    ///     omega_d             1/ns
    ///     filtered derivative signal units/ns
    ///
    /// Only the final division narrows the new state.
    template <is_fixed dt_ns_fixed_t>
        requires(!is_signed_v<dt_ns_fixed_t>)
    constexpr auto operator()(x_t input, x_t previous_filtered_input, cutoff_rate_t derivative_cutoff_rate,
        dt_ns_fixed_t dt_ns) noexcept -> dx_t
    {
        // Velocity magnitude is physically nonnegative. Consequently,
        // input - previous_filtered_input fits x_t even though x_t itself
        // is signed.
        assert(input >= x_t{});
        assert(previous_filtered_input >= x_t{});

        auto const delta = input - previous_filtered_input;
        auto const cutoff_step_product = multiply(derivative_cutoff_rate, dt_ns);
        auto const cutoff_step = limit_cutoff_step_(cutoff_step_product);

        if (cutoff_step.saturated)
        {
            // as dt -> infinity => (d_prev + omega*delta)/(1 + omega*dt) -> 0
            output_ = dx_t{};
            return output_;
        }

        auto const weighted_delta = multiply(derivative_cutoff_rate, delta);

        using numerator_t = decltype(weighted_delta);
        static_assert(
            numerator_t::frac_bits >= dx_t::frac_bits, "derivative numerator must represent derivative state exactly");

        auto const previous_aligned = numerator_t::convert(output_);
        auto const numerator = previous_aligned + weighted_delta;
        auto const denominator = cutoff_step.value + cutoff_step_t{1};
        output_ = detail::divide_rne<dx_t>(numerator, denominator);

        return output_;
    }

private:
    dx_t output_{};

    [[no_unique_address]]
    cutoff_step_limiter_t limit_cutoff_step_{};
};

/// calculates the adaptive signal cutoff step
///
///     u = (minimum_cutoff_rate + cutoff_slope*abs(filtered_derivative))*dt
///
/// Runtime units:
///
///     minimum_cutoff_rate  1/ns
///     filtered derivative  signal units/ns
///     cutoff_slope         1/signal-unit
///     dt                   ns
///     u                    dimensionless
///
/// `cutoff_slope` is 2*pi times paper beta when the paper derivative is interpreted in signal units/second.
template <is_fixed t_cutoff_rate_t, is_fixed t_cutoff_slope_t, is_fixed t_cutoff_step_t,
    typename t_cutoff_rate_combiner_t = cutoff_rate_combiner_t<t_cutoff_rate_t>,
    typename t_cutoff_step_limiter_t = cutoff_step_limiter_t<t_cutoff_step_t>>
    requires(!is_signed_v<t_cutoff_rate_t> && !is_signed_v<t_cutoff_slope_t> && !is_signed_v<t_cutoff_step_t>)
class cutoff_step_calculator_t
{
public:
    using cutoff_rate_t = t_cutoff_rate_t;
    using cutoff_slope_t = t_cutoff_slope_t;
    using cutoff_step_t = t_cutoff_step_t;
    using cutoff_rate_combiner_t = t_cutoff_rate_combiner_t;
    using cutoff_step_limiter_t = t_cutoff_step_limiter_t;
    using output_t = bounded_cutoff_step_t<cutoff_step_t>;

    constexpr cutoff_step_calculator_t() noexcept = default;

    constexpr explicit cutoff_step_calculator_t(
        cutoff_rate_combiner_t combine_cutoff_rates, cutoff_step_limiter_t limit_cutoff_step = {}) noexcept
        : combine_cutoff_rates_{std::move(combine_cutoff_rates)}, limit_cutoff_step_{std::move(limit_cutoff_step)}
    {}

    template <is_fixed dx_t, is_fixed dt_ns_fixed_t>
        requires(is_signed_v<dx_t> && !is_signed_v<dt_ns_fixed_t>)
    constexpr auto operator()(cutoff_rate_t minimum_cutoff_rate, cutoff_slope_t cutoff_slope, dx_t filtered_derivative,
        dt_ns_fixed_t dt_ns) const noexcept -> output_t
    {
        auto const adaptive_cutoff_rate = multiply(cutoff_slope, uabs(filtered_derivative));
        auto const combined_cutoff_rate = combine_cutoff_rates_(minimum_cutoff_rate, adaptive_cutoff_rate);
        auto const cutoff_step_product = multiply(combined_cutoff_rate, dt_ns);

        return limit_cutoff_step_(cutoff_step_product);
    }

private:
    [[no_unique_address]] cutoff_rate_combiner_t combine_cutoff_rates_{};
    [[no_unique_address]] cutoff_step_limiter_t limit_cutoff_step_{};
};

/// low-pass filter for input signal
///
/// Implements:
///
///     alpha = u/(1 + u)
///     filtered_x = filtered_x_prev + alpha*(x - filtered_x_prev)
///
/// Rearranged:
///
///     filtered_x = (filtered_x_prev + u*x)/(1 + u)
///
/// This avoids materializing alpha and avoids narrowing an intermediate EMA correction. Only the final division narrows
/// the new state.
template <is_fixed t_x_t, is_fixed t_cutoff_step_t>
    requires(is_signed_v<t_x_t> && !is_signed_v<t_cutoff_step_t>)
class signal_low_pass_t
{
public:
    using x_t = t_x_t;
    using cutoff_step_t = t_cutoff_step_t;
    using bounded_step_t = bounded_cutoff_step_t<cutoff_step_t>;

    constexpr signal_low_pass_t() noexcept = default;
    constexpr explicit signal_low_pass_t(x_t initial) noexcept : output_{initial} {}

    constexpr auto output() const noexcept -> x_t { return output_; }
    constexpr void reset(x_t initial = {}) noexcept { output_ = initial; }

    constexpr auto operator()(x_t input, bounded_step_t cutoff_step) noexcept -> x_t
    {
        assert(input >= x_t{});
        assert(output_ >= x_t{});

        if (cutoff_step.saturated)
        {
            // As u -> infinity, alpha -> 1.
            output_ = input;
            return output_;
        }

        auto const weighted_input = multiply(cutoff_step.value, input);
        using numerator_t = decltype(weighted_input);
        static_assert(numerator_t::frac_bits >= x_t::frac_bits, "signal numerator must represent signal state exactly");
        auto const previous_aligned = numerator_t::convert(output_);

        // cutoff_step_limiter_t reserves room for +1. Since both input and output are nonnegative and no greater than
        // max<x_t>(), that same reservation also leaves room for cutoff_step*input + previous_output
        auto const numerator = weighted_input + previous_aligned;
        auto const denominator = cutoff_step.value + cutoff_step_t{1};

        output_ = detail::divide_rne<x_t>(numerator, denominator);

        return output_;
    }

private:
    x_t output_{};
};

/// Variable-interval fixed-point 1-Euro filter over velocity magnitude.
///
/// This implements Casiez, Roussel, and Vogel, “1 € Filter: A Simple Speed-based Low-pass Filter for Noisy Input in
/// Interactive Systems,” CHI 2012.
///
/// The signal is nonnegative velocity magnitude stored in a signed fixed-point type for compatibility with the
/// downstream spline evaluator. Its filtered derivative is signed.
///
/// The frontend preconverts user-facing parameters:
///
///     derivative_cutoff_rate = ln(2)/derivative_half_life_ns
///     minimum_cutoff_rate = ln(2)/minimum_half_life_ns
///     cutoff_slope = 2*pi*paper_beta
///
/// Runtime units:
///
///     input                   counts/ms
///     dt_ns                   ns
///     derivative state        counts/ms/ns
///     cutoff rates            1/ns
///     cutoff slope            1/(counts/ms)
///
/// The first sample seeds the signal state and clears the derivative state. No previous raw input is stored.
template <is_fixed t_x_t, is_fixed t_dx_t, is_fixed t_cutoff_rate_t, is_fixed t_cutoff_slope_t,
    is_fixed t_cutoff_step_t,
    typename t_derivative_low_pass_t = derivative_low_pass_t<t_x_t, t_dx_t, t_cutoff_rate_t, t_cutoff_step_t>,
    typename t_cutoff_step_calculator_t = cutoff_step_calculator_t<t_cutoff_rate_t, t_cutoff_slope_t, t_cutoff_step_t>,
    typename t_signal_low_pass_t = signal_low_pass_t<t_x_t, t_cutoff_step_t>>
    requires(is_signed_v<t_x_t> && is_signed_v<t_dx_t> && !is_signed_v<t_cutoff_rate_t>
        && !is_signed_v<t_cutoff_slope_t> && !is_signed_v<t_cutoff_step_t> && !is_fixed_frac<t_cutoff_step_t>)
class filter_t
{
public:
    using x_t = t_x_t;
    using dx_t = t_dx_t;
    using cutoff_rate_t = t_cutoff_rate_t;
    using cutoff_slope_t = t_cutoff_slope_t;
    using cutoff_step_t = t_cutoff_step_t;
    using derivative_low_pass_t = t_derivative_low_pass_t;
    using cutoff_step_calculator_t = t_cutoff_step_calculator_t;
    using signal_low_pass_t = t_signal_low_pass_t;

    using dt_ns_t = uint64_t;
    using dt_ns_fixed_t = fixed_t<uint64_t, 0>;

    struct params_t
    {
        /// Angular derivative cutoff rate in 1/ns.
        cutoff_rate_t derivative_cutoff_rate;

        /// Minimum angular signal cutoff rate in 1/ns.
        cutoff_rate_t minimum_cutoff_rate;

        /// Angular cutoff slope per unit of runtime derivative.
        ///
        /// For paper-style beta:
        ///
        ///     cutoff_slope = 2*pi*beta
        cutoff_slope_t cutoff_slope;
    };

    constexpr explicit filter_t(params_t params) noexcept : params_{params} {}

    /// Dependency/state injection constructor.
    ///
    /// When initialized is true, the injected low-pass objects are expected to contain the complete current filter
    /// state.
    constexpr explicit filter_t(params_t params, derivative_low_pass_t derivative_low_pass,
        cutoff_step_calculator_t cutoff_step_calculator, signal_low_pass_t signal_low_pass, bool initialized) noexcept
        : params_{params}, derivative_low_pass_{std::move(derivative_low_pass)},
          cutoff_step_calculator_{std::move(cutoff_step_calculator)}, signal_low_pass_{std::move(signal_low_pass)},
          initialized_{initialized}
    {}

    /// Discards all filter history.
    ///
    /// The next sample seeds the signal state and clears the derivative state.
    constexpr void reset() noexcept
    {
        derivative_low_pass_.reset();
        signal_low_pass_.reset();
        initialized_ = false;
    }

    /// Filters one velocity sample.
    ///
    /// dt_ns is ignored for the first sample after construction or reset.
    constexpr auto operator()(x_t input, dt_ns_t dt_ns) noexcept -> x_t
    {
        assert(input >= x_t{});

        if (!initialized_) [[unlikely]]
        {
            derivative_low_pass_.reset(dx_t{});
            signal_low_pass_.reset(input);
            initialized_ = true;
            return input;
        }

        assert(dt_ns != 0);

        auto const dt = dt_ns_fixed_t::literal(dt_ns);
        auto const previous_filtered_input = signal_low_pass_.output();
        auto const filtered_derivative
            = derivative_low_pass_(input, previous_filtered_input, params_.derivative_cutoff_rate, dt);
        auto const signal_cutoff_step
            = cutoff_step_calculator_(params_.minimum_cutoff_rate, params_.cutoff_slope, filtered_derivative, dt);

        return signal_low_pass_(input, signal_cutoff_step);
    }

    constexpr auto signal_state() const noexcept -> x_t { return signal_low_pass_.output(); }
    constexpr auto derivative_state() const noexcept -> dx_t { return derivative_low_pass_.output(); }
    constexpr auto initialized() const noexcept -> bool { return initialized_; }

private:
    params_t params_;
    derivative_low_pass_t derivative_low_pass_{};
    [[no_unique_address]] cutoff_step_calculator_t cutoff_step_calculator_{};
    signal_low_pass_t signal_low_pass_{};
    bool initialized_{};
};

} // namespace crv::pipeline::filters::one_euro
