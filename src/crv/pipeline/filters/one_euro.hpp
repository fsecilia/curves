// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/fma.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/limits.hpp>
#include <crv/math/rounding_mode.hpp>
#include <crv/math/shifter.hpp>
#include <crv/pipeline/filters/one_euro/cutoff_interval.hpp>
#include <crv/pipeline/filters/one_euro/signal_cutoff_rate.hpp>
#include <cassert>
#include <expected>
#include <utility>

namespace crv::pipeline::filters::one_euro {

namespace detail {

using rne_shifter_t = shifter_t<rounding_modes::shr::nearest_even>;

} // namespace detail

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
    template <is_fixed t_dx_t,
        typename t_signal_cutoff_rate_calculator_t = signal_cutoff_rate_calculator_t<cutoff_rate_t>>
        requires(is_signed_v<t_dx_t>)
    constexpr auto validate(t_signal_cutoff_rate_calculator_t const& signal_cutoff_rate_calculator = {}) const noexcept
        -> std::expected<void, validation_error>
    {
        using dx_t = t_dx_t;

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
    using numerator_fma_t
        = fma_t<numerator_t, cutoff_rate_t, x_t, dx_t, detail::rne_shifter_t, fixed::overflow_policy_t::saturate>;

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
            // alpha_d -> 1, so the derivative state approaches the current raw derivative.
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

/// Low-pass filter for the input signal.
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
/// The same quantized finite cutoff interval is used in the numerator and denominator.
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
    using numerator_fma_t
        = fma_t<numerator_t, cutoff_interval_t, x_t, x_t, detail::rne_shifter_t, fixed::overflow_policy_t::saturate>;

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
            // alpha -> 1
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

/// Variable-interval fixed-point 1-Euro filter.
///
/// This implementation follows the maintained reference implementation's derivative baseline: each derivative sample is
/// measured from the previous filtered signal rather than the previous raw signal.
///
/// For each sample after initialization:
///
///     previous_filtered_input = signal_filter.output()
///     filtered_derivative
///         = derivative_filter(input, previous_filtered_input, derivative_cutoff_rate, dt_ns)
///     cutoff_rate
///         = minimum_cutoff_rate + cutoff_slope*abs(filtered_derivative)
///     filtered_input
///         = signal_filter(input, cutoff_rate, dt_ns)
///
/// The ordinary constructor creates an uninitialized filter. Its first input seeds the signal state and clears the
/// derivative state.
///
/// The dependency/state injection constructor instead accepts complete recursive state and is immediately initialized.
///
/// \pre input >= 0
/// \pre dt_ns > 0
template <is_fixed t_x_t, is_fixed t_dx_t, typename t_params_t, typename t_derivative_filter_t,
    typename t_signal_cutoff_rate_calculator_t, typename t_signal_filter_t>
    requires(is_signed_v<t_x_t> && is_signed_v<t_dx_t>)
class filter_t
{
public:
    using x_t = t_x_t;
    using dx_t = t_dx_t;
    using derivative_filter_t = t_derivative_filter_t;
    using signal_cutoff_rate_calculator_t = t_signal_cutoff_rate_calculator_t;
    using signal_filter_t = t_signal_filter_t;
    using params_t = t_params_t;

    using cutoff_rate_t = params_t::cutoff_rate_t;
    using cutoff_slope_t = params_t::cutoff_slope_t;

    /// Constructs a new filter with no recursive history.
    constexpr explicit filter_t(params_t params) noexcept : params_{params}
    {
        assert(params_.template validate<dx_t>());
    }

    /// Constructs an initialized filter from complete recursive component state.
    constexpr explicit filter_t(params_t params, derivative_filter_t derivative_filter,
        signal_cutoff_rate_calculator_t signal_cutoff_rate_calculator, signal_filter_t signal_filter) noexcept
        : derivative_filter_{std::move(derivative_filter)},
          signal_cutoff_rate_calculator_{std::move(signal_cutoff_rate_calculator)},
          signal_filter_{std::move(signal_filter)}, params_{params}, initialized_{true}
    {
        assert(params_.template validate<dx_t>());
    }

    template <is_fixed dt_ns_t>
        requires(!is_signed_v<dt_ns_t>)
    constexpr auto operator()(x_t input, dt_ns_t dt_ns) noexcept -> x_t
    {
        assert(input >= x_t{});
        assert(dt_ns > dt_ns_t{});

        if (!initialized_) [[unlikely]]
        {
            derivative_filter_.reset();
            signal_filter_.reset(input);
            initialized_ = true;
            return input;
        }

        auto const previous_filtered_input = signal_filter_.output();

        auto const filtered_derivative
            = derivative_filter_(input, previous_filtered_input, params_.derivative_cutoff_rate, dt_ns);

        auto const cutoff_rate = signal_cutoff_rate_calculator_.calc(
            params_.minimum_cutoff_rate, params_.cutoff_slope, filtered_derivative);

        return signal_filter_(input, cutoff_rate, dt_ns);
    }

private:
    [[no_unique_address]] derivative_filter_t derivative_filter_{};
    [[no_unique_address]] signal_cutoff_rate_calculator_t signal_cutoff_rate_calculator_{};
    [[no_unique_address]] signal_filter_t signal_filter_{};
    params_t params_;
    bool initialized_{};
};

} // namespace crv::pipeline::filters::one_euro
