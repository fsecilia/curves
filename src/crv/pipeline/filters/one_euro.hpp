// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/fma.hpp>
#include <crv/math/fixed/uabs.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/limits.hpp>
#include <crv/math/rounding_mode.hpp>
#include <crv/math/shifter.hpp>
#include <crv/pipeline/filters/one_euro/cutoff_interval.hpp>
#include <cassert>
#include <expected>
#include <type_traits>
#include <utility>

namespace crv::pipeline::filters::one_euro {

namespace detail {

using rne_shifter_t = shifter_t<rounding_modes::shr::nearest_even>;
using truncate_shifter_t = shifter_t<rounding_modes::shr::truncate>;

static constexpr auto rne_shifter = rne_shifter_t{};
static constexpr auto truncate_shifter = truncate_shifter_t{};

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
        .value = cutoff_rate_t::template convert<rne_shifter>(combined),
    };
}

template <is_fixed cutoff_rate_t, is_fixed cutoff_slope_t, is_fixed dx_t>
    requires(is_signed_v<cutoff_rate_t> && is_signed_v<cutoff_slope_t> && is_signed_v<dx_t>)
constexpr auto signal_cutoff_rate_overflows(
    cutoff_rate_t minimum_cutoff_rate, cutoff_slope_t cutoff_slope, dx_t filtered_derivative) noexcept -> bool
{
    return try_signal_cutoff_rate(minimum_cutoff_rate, cutoff_slope, filtered_derivative).overflows;
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
struct parameters_t
{
    using cutoff_rate_t = t_cutoff_rate_t;
    using cutoff_slope_t = t_cutoff_slope_t;

    cutoff_rate_t derivative_cutoff_rate;
    cutoff_rate_t minimum_cutoff_rate;
    cutoff_slope_t cutoff_slope;
};

enum class validation_error
{
    derivative_cutoff_rate_not_positive,
    minimum_cutoff_rate_not_positive,
    cutoff_slope_negative,
    signal_cutoff_rate_overflow,
};

/// Validates runtime parameters over every representable derivative state.
///
/// Construction alone cannot establish this invariant because parameter objects may arrive through external storage
/// such as an ioctl payload.
///
/// The adaptive-cutoff check covers:
///
///     [min<dx_t>(), max<dx_t>()]
///
/// and therefore uses `min<dx_t>()`, whose two's-complement magnitude is the larger endpoint.
template <is_fixed t_dx_t, is_fixed cutoff_rate_t, is_fixed cutoff_slope_t>
    requires(is_signed_v<t_dx_t> && is_signed_v<cutoff_rate_t> && is_signed_v<cutoff_slope_t>)
constexpr auto validate(parameters_t<cutoff_rate_t, cutoff_slope_t> const& params) noexcept
    -> std::expected<void, validation_error>
{
    using dx_t = t_dx_t;

    if (params.derivative_cutoff_rate <= cutoff_rate_t{})
    {
        return std::unexpected{validation_error::derivative_cutoff_rate_not_positive};
    }

    if (params.minimum_cutoff_rate <= cutoff_rate_t{})
    {
        return std::unexpected{validation_error::minimum_cutoff_rate_not_positive};
    }

    if (params.cutoff_slope < cutoff_slope_t{}) { return std::unexpected{validation_error::cutoff_slope_negative}; }

    if (detail::signal_cutoff_rate_overflows(params.minimum_cutoff_rate, params.cutoff_slope, min<dx_t>()))
    {
        return std::unexpected{validation_error::signal_cutoff_rate_overflow};
    }

    return {};
}

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
template <is_fixed t_x_t, is_fixed t_dx_t, is_fixed t_cutoff_rate_t, is_fixed t_cutoff_interval_value_t>
    requires(is_signed_v<t_x_t> && is_signed_v<t_dx_t> && is_signed_v<t_cutoff_rate_t>
        && is_signed_v<t_cutoff_interval_value_t>)
class derivative_filter_t
{
public:
    using x_t = t_x_t;
    using dx_t = t_dx_t;
    using cutoff_rate_t = t_cutoff_rate_t;
    using cutoff_interval_value_t = t_cutoff_interval_value_t;

    using numerator_t = fixed::product_t<cutoff_rate_t, x_t>;
    using numerator_fma_t
        = fma_t<numerator_t, cutoff_rate_t, x_t, dx_t, detail::rne_shifter_t, fixed::overflow_policy_t::saturate>;

    // this constraint competes with the divider's requirement that shifts only be right; the net effect is dx_t = x_t
    static_assert(
        dx_t::int_bits >= x_t::int_bits, "derivative state must contain the maximum raw derivative at dt_ns == 1");

    static_assert(
        numerator_t::int_bits >= dx_t::int_bits, "derivative numerator must contain the derivative state range");

    static_assert(
        numerator_t::frac_bits >= dx_t::frac_bits, "derivative numerator must contain the derivative state precision");

    // The FMA output representation has enough headroom for every representable operand combination. Saturation is a
    // defensive policy only; it must never be reachable.
    static_assert(!numerator_fma_t::lower_saturation_possible, "derivative numerator FMA can saturate");
    static_assert(!numerator_fma_t::upper_saturation_possible, "derivative numerator FMA can saturate");

    constexpr derivative_filter_t() noexcept = default;
    constexpr explicit derivative_filter_t(dx_t initial) noexcept : output_{initial} {}

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
        auto const interval = make_cutoff_interval<cutoff_interval_value_t>(derivative_cutoff_rate, dt_ns);

        if (interval.is_limit)
        {
            // alpha_d -> 1, so the derivative state approaches the current raw derivative.
            output_ = divide<dx_t>(delta, dt_ns, rounding_modes::div::nearest_even);
            return output_;
        }

        auto const numerator = numerator_fma_t{}(derivative_cutoff_rate, delta, output_);
        auto const denominator = interval.value + cutoff_interval_value_t{1};

        output_ = divide<dx_t>(numerator, denominator, rounding_modes::div::nearest_even);
        return output_;
    }

private:
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
template <is_fixed t_x_t, is_fixed t_cutoff_rate_t, is_fixed t_cutoff_interval_value_t>
    requires(is_signed_v<t_x_t> && is_signed_v<t_cutoff_rate_t> && is_signed_v<t_cutoff_interval_value_t>)
class signal_filter_t
{
public:
    using x_t = t_x_t;
    using cutoff_rate_t = t_cutoff_rate_t;
    using cutoff_interval_value_t = t_cutoff_interval_value_t;

    using numerator_t = fixed::product_t<cutoff_interval_value_t, x_t>;
    using numerator_fma_t = fma_t<numerator_t, cutoff_interval_value_t, x_t, x_t, detail::rne_shifter_t,
        fixed::overflow_policy_t::saturate>;

    static_assert(numerator_t::int_bits >= x_t::int_bits, "signal numerator must contain the signal state range");

    static_assert(numerator_t::frac_bits >= x_t::frac_bits, "signal numerator must contain the signal state precision");

    // As above, saturation is retained defensively but must be statically unreachable.
    static_assert(!numerator_fma_t::lower_saturation_possible, "signal numerator FMA can saturate");
    static_assert(!numerator_fma_t::upper_saturation_possible, "signal numerator FMA can saturate");

    constexpr signal_filter_t() noexcept = default;
    constexpr explicit signal_filter_t(x_t initial) noexcept : output_{initial} {}

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

        auto const interval = make_cutoff_interval<cutoff_interval_value_t>(cutoff_rate, dt_ns);

        if (interval.is_limit)
        {
            // alpha -> 1
            output_ = input;
            return output_;
        }

        auto const numerator = numerator_fma_t{}(interval.value, input, output_);
        auto const denominator = interval.value + cutoff_interval_value_t{1};

        output_ = divide<x_t>(numerator, denominator, rounding_modes::div::nearest_even);
        return output_;
    }

private:
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
template <is_fixed t_x_t, is_fixed t_dx_t, is_fixed t_cutoff_rate_t, is_fixed t_cutoff_slope_t,
    is_fixed t_cutoff_interval_value_t,
    typename t_derivative_filter_t = derivative_filter_t<t_x_t, t_dx_t, t_cutoff_rate_t, t_cutoff_interval_value_t>,
    typename t_signal_filter_t = signal_filter_t<t_x_t, t_cutoff_rate_t, t_cutoff_interval_value_t>>
    requires(is_signed_v<t_x_t> && is_signed_v<t_dx_t> && is_signed_v<t_cutoff_rate_t> && is_signed_v<t_cutoff_slope_t>
        && is_signed_v<t_cutoff_interval_value_t>)
class filter_t
{
public:
    using x_t = t_x_t;
    using dx_t = t_dx_t;
    using cutoff_rate_t = t_cutoff_rate_t;
    using cutoff_slope_t = t_cutoff_slope_t;
    using cutoff_interval_value_t = t_cutoff_interval_value_t;

    using derivative_filter_type = t_derivative_filter_t;
    using signal_filter_type = t_signal_filter_t;

    using params_t = parameters_t<cutoff_rate_t, cutoff_slope_t>;

    /// Constructs a new filter with no recursive history.
    constexpr explicit filter_t(params_t params) noexcept : params_{params}
    {
        assert((validate<dx_t>(params_).has_value()));
    }

    /// Constructs an initialized filter from complete recursive component state.
    constexpr explicit filter_t(
        params_t params, derivative_filter_type derivative_filter, signal_filter_type signal_filter) noexcept
        : params_{params}, derivative_filter_{std::move(derivative_filter)}, signal_filter_{std::move(signal_filter)},
          initialized_{true}
    {
        assert((validate<dx_t>(params_).has_value()));
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

        auto const cutoff_rate
            = detail::signal_cutoff_rate(params_.minimum_cutoff_rate, params_.cutoff_slope, filtered_derivative);

        return signal_filter_(input, cutoff_rate, dt_ns);
    }

private:
    params_t params_;

    [[no_unique_address]]
    derivative_filter_type derivative_filter_{};

    [[no_unique_address]]
    signal_filter_type signal_filter_{};

    bool initialized_{};
};

} // namespace crv::pipeline::filters::one_euro
