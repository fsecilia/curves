// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/fma.hpp>
#include <crv/math/fixed/uabs.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/rounding_mode.hpp>
#include <crv/math/shifter.hpp>

#include <cassert>
#include <expected>
#include <type_traits>
#include <utility>

namespace crv::pipeline::filters::one_euro {

namespace detail {

using rne_shifter_t = shifter_t<rounding_modes::shr::nearest_even>;

static constexpr auto rne_shifter = rne_shifter_t{};

/// Finite dimensionless cutoff interval or its unrepresentably large limit.
///
/// For cutoff rate k and elapsed time dt:
///
///     value = k*dt
///
/// A finite value reserves room for the recurrence denominator:
///
///     1 + k*dt
///
/// When is_limit is true, the caller evaluates its recurrence's mathematical
/// k*dt -> infinity limit instead.
template <is_fixed t_value_t>
    requires(is_signed_v<t_value_t> && !is_fixed_frac<t_value_t>)
struct cutoff_interval_t
{
    using value_t = t_value_t;

    value_t value{};
    bool is_limit{};
};

/// Forms the dimensionless cutoff interval k*dt.
///
/// cutoff_interval_value_t must preserve at least the cutoff rate's fractional
/// precision. Since dt_ns is a positive integer, a positive representable
/// cutoff rate therefore cannot underflow to zero while forming k*dt.
///
/// Products too large to leave room for the denominator's +1 are represented
/// by the limiting case rather than numerically saturated.
template <is_fixed t_cutoff_interval_value_t, is_fixed cutoff_rate_t, is_fixed dt_ns_t>
    requires(is_signed_v<t_cutoff_interval_value_t> && is_signed_v<cutoff_rate_t> && !is_signed_v<dt_ns_t>
        && !is_fixed_frac<t_cutoff_interval_value_t> && !is_fixed_frac<dt_ns_t>)
constexpr auto make_cutoff_interval(cutoff_rate_t cutoff_rate, dt_ns_t dt_ns) noexcept
    -> cutoff_interval_t<t_cutoff_interval_value_t>
{
    using cutoff_interval_value_t = t_cutoff_interval_value_t;
    using output_t = cutoff_interval_t<cutoff_interval_value_t>;

    static_assert(cutoff_interval_value_t::frac_bits >= cutoff_rate_t::frac_bits);

    assert(cutoff_rate > cutoff_rate_t{});
    assert(dt_ns > dt_ns_t{});

    static constexpr auto finite_ceiling = max<cutoff_interval_value_t>() - cutoff_interval_value_t{1};

    auto const product = multiply(cutoff_rate, dt_ns);
    using product_t = std::remove_cvref_t<decltype(product)>;

    static constexpr auto product_ceiling = product_t::convert(finite_ceiling);

    if (product > product_ceiling) return output_t{.is_limit = true};

    auto const value = cutoff_interval_value_t::template convert<rne_shifter>(product);

    // cutoff_rate > 0, dt_ns >= 1, and the output preserves at least the
    // cutoff rate's fractional precision.
    assert(value > cutoff_interval_value_t{});

    return output_t{.value = value};
}

/// Returns whether the adaptive signal cutoff would exceed cutoff_rate_t.
///
/// Calculates:
///
///     minimum_cutoff_rate + cutoff_slope*abs(filtered_derivative)
///
/// without first performing the potentially overflowing addition.
template <is_fixed cutoff_rate_t, is_fixed cutoff_slope_t, is_fixed dx_t>
    requires(is_signed_v<cutoff_rate_t> && is_signed_v<cutoff_slope_t> && is_signed_v<dx_t>)
constexpr auto signal_cutoff_rate_overflows(
    cutoff_rate_t minimum_cutoff_rate, cutoff_slope_t cutoff_slope, dx_t filtered_derivative) noexcept -> bool
{
    assert(minimum_cutoff_rate > cutoff_rate_t{});
    assert(cutoff_slope >= cutoff_slope_t{});

    auto const adaptive_cutoff_rate = multiply(cutoff_slope, uabs(filtered_derivative));

    using adaptive_cutoff_rate_t = std::remove_cvref_t<decltype(adaptive_cutoff_rate)>;

    using accumulator_t = fixed_t<typename adaptive_cutoff_rate_t::value_t, adaptive_cutoff_rate_t::frac_bits>;

    static_assert(is_signed_v<accumulator_t>);
    static_assert(cutoff_rate_t::frac_bits <= accumulator_t::frac_bits);
    static_assert(cutoff_rate_t::int_bits <= accumulator_t::int_bits);

    auto const adaptive = accumulator_t::convert(adaptive_cutoff_rate);
    auto const minimum = accumulator_t::convert(minimum_cutoff_rate);
    auto const output_max = accumulator_t::convert(max<cutoff_rate_t>());

    return adaptive > output_max - minimum;
}

/// Calculates the adaptive signal cutoff.
///
/// Preconditions are independently checkable by signal_cutoff_rate_overflows().
template <is_fixed cutoff_rate_t, is_fixed cutoff_slope_t, is_fixed dx_t>
    requires(is_signed_v<cutoff_rate_t> && is_signed_v<cutoff_slope_t> && is_signed_v<dx_t>)
constexpr auto signal_cutoff_rate(
    cutoff_rate_t minimum_cutoff_rate, cutoff_slope_t cutoff_slope, dx_t filtered_derivative) noexcept -> cutoff_rate_t
{
    assert(!signal_cutoff_rate_overflows(minimum_cutoff_rate, cutoff_slope, filtered_derivative));

    auto const adaptive_cutoff_rate = multiply(cutoff_slope, uabs(filtered_derivative));

    using adaptive_cutoff_rate_t = std::remove_cvref_t<decltype(adaptive_cutoff_rate)>;

    using accumulator_t = fixed_t<typename adaptive_cutoff_rate_t::value_t, adaptive_cutoff_rate_t::frac_bits>;

    auto const combined = accumulator_t::convert(minimum_cutoff_rate) + accumulator_t::convert(adaptive_cutoff_rate);

    return cutoff_rate_t::template convert<rne_shifter>(combined);
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
/// The filtered derivative uses signal-units/ns, so:
///
///     minimum_cutoff_rate
///         + cutoff_slope*abs(filtered_derivative)
///
/// has units 1/ns.
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

/// Validates runtime parameters.
///
/// Validation is independent of object construction so the same operation can
/// be used for user-mode input and for an already-constructed ioctl payload.
///
/// The cutoff overflow check covers every representable derivative state.
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

    // This is one raw unit more conservative than the physically reachable
    // symmetric derivative domain when dx_t uses two's-complement storage,
    // but proves the parameter set for every representable dx_t state.
    if (detail::signal_cutoff_rate_overflows(params.minimum_cutoff_rate, params.cutoff_slope, min<dx_t>()))
    {
        return std::unexpected{validation_error::signal_cutoff_rate_overflow};
    }

    return {};
}

/// Low-pass filter for the signal derivative.
///
/// The reference calculation is:
///
///     raw_derivative =
///         (input - previous_filtered_input) / dt
///
///     alpha_d =
///         derivative_cutoff_rate*dt
///         / (1 + derivative_cutoff_rate*dt)
///
///     filtered_derivative =
///         previous_filtered_derivative
///         + alpha_d*(raw_derivative - previous_filtered_derivative)
///
/// Rearranging gives:
///
///     filtered_derivative =
///         (previous_filtered_derivative
///             + derivative_cutoff_rate
///                 * (input - previous_filtered_input))
///         / (1 + derivative_cutoff_rate*dt)
///
/// This eliminates the raw derivative division and does not materialize alpha.
template <is_fixed t_x_t, is_fixed t_dx_t, is_fixed t_cutoff_rate_t, is_fixed t_cutoff_interval_value_t>
    requires(is_signed_v<t_x_t> && is_signed_v<t_dx_t> && is_signed_v<t_cutoff_rate_t>
        && is_signed_v<t_cutoff_interval_value_t> && !is_fixed_frac<t_cutoff_interval_value_t>)
class derivative_filter_t
{
public:
    using x_t = t_x_t;
    using dx_t = t_dx_t;
    using cutoff_rate_t = t_cutoff_rate_t;
    using cutoff_interval_value_t = t_cutoff_interval_value_t;

    using numerator_fma_t = fma_t<fixed::product_t<cutoff_rate_t, x_t>, cutoff_rate_t, x_t, dx_t, detail::rne_shifter_t,
        fixed::overflow_policy_t::saturate>;

    using numerator_t = typename numerator_fma_t::out_t;

    static_assert(is_fixed<numerator_t>);
    static_assert(is_signed_v<numerator_t>);

    static_assert(
        numerator_t::int_bits >= dx_t::int_bits, "derivative numerator must contain the derivative state range");

    static_assert(
        numerator_t::frac_bits >= dx_t::frac_bits, "derivative numerator must contain the derivative state precision");

    constexpr derivative_filter_t() noexcept = default;

    constexpr explicit derivative_filter_t(dx_t initial) noexcept : output_{initial} {}

    constexpr void reset(dx_t initial = {}) noexcept { output_ = initial; }

    template <is_fixed dt_ns_t>
        requires(!is_signed_v<dt_ns_t> && !is_fixed_frac<dt_ns_t>)
    constexpr auto operator()(
        x_t input, x_t previous_filtered_input, cutoff_rate_t derivative_cutoff_rate, dt_ns_t dt_ns) noexcept -> dx_t
    {
        assert(input >= x_t{});
        assert(previous_filtered_input >= x_t{});
        assert(derivative_cutoff_rate > cutoff_rate_t{});
        assert(dt_ns > dt_ns_t{});

        // Both operands lie in x_t's nonnegative range, so their difference
        // lies in [-max<x_t>(), max<x_t>()] and remains representable in x_t.
        auto const delta = input - previous_filtered_input;

        auto const interval = detail::make_cutoff_interval<cutoff_interval_value_t>(derivative_cutoff_rate, dt_ns);

        if (interval.is_limit)
        {
            // As k_d*dt -> infinity, alpha_d -> 1 and the derivative leg
            // approaches the current raw derivative.
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
/// The reference calculation is:
///
///     alpha = cutoff_rate*dt / (1 + cutoff_rate*dt)
///
///     filtered =
///         previous_filtered
///         + alpha*(input - previous_filtered)
///
/// Rearranging gives:
///
///     filtered =
///         (previous_filtered + cutoff_rate*dt*input)
///         / (1 + cutoff_rate*dt)
///
/// The finite implementation materializes cutoff_rate*dt once in the narrow
/// denominator representation, then uses that same value in the numerator.
template <is_fixed t_x_t, is_fixed t_cutoff_rate_t, is_fixed t_cutoff_interval_value_t>
    requires(is_signed_v<t_x_t> && is_signed_v<t_cutoff_rate_t> && is_signed_v<t_cutoff_interval_value_t>
        && !is_fixed_frac<t_cutoff_interval_value_t>)
class signal_filter_t
{
public:
    using x_t = t_x_t;
    using cutoff_rate_t = t_cutoff_rate_t;
    using cutoff_interval_value_t = t_cutoff_interval_value_t;

    using numerator_fma_t = fma_t<fixed::product_t<cutoff_interval_value_t, x_t>, cutoff_interval_value_t, x_t, x_t,
        detail::rne_shifter_t, fixed::overflow_policy_t::saturate>;

    using numerator_t = typename numerator_fma_t::out_t;

    static_assert(is_fixed<numerator_t>);
    static_assert(is_signed_v<numerator_t>);

    static_assert(numerator_t::int_bits >= x_t::int_bits, "signal numerator must contain the signal state range");

    static_assert(numerator_t::frac_bits >= x_t::frac_bits, "signal numerator must contain the signal state precision");

    constexpr signal_filter_t() noexcept = default;

    constexpr explicit signal_filter_t(x_t initial) noexcept : output_{initial} {}

    constexpr auto output() const noexcept -> x_t { return output_; }

    constexpr void reset(x_t initial = {}) noexcept { output_ = initial; }

    template <is_fixed dt_ns_t>
        requires(!is_signed_v<dt_ns_t> && !is_fixed_frac<dt_ns_t>)
    constexpr auto operator()(x_t input, cutoff_rate_t cutoff_rate, dt_ns_t dt_ns) noexcept -> x_t
    {
        assert(input >= x_t{});
        assert(output_ >= x_t{});
        assert(cutoff_rate > cutoff_rate_t{});
        assert(dt_ns > dt_ns_t{});

        auto const interval = detail::make_cutoff_interval<cutoff_interval_value_t>(cutoff_rate, dt_ns);

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
/// The first sample initializes the reference state:
///
///     filtered_derivative = 0
///     filtered_signal     = input
///
/// Subsequent samples preserve reference update order:
///
///     filtered_derivative = derivative_filter(...)
///     cutoff              = minimum + slope*abs(filtered_derivative)
///     filtered_signal     = signal_filter(...)
template <is_fixed t_x_t, is_fixed t_dx_t, is_fixed t_cutoff_rate_t, is_fixed t_cutoff_slope_t,
    is_fixed t_cutoff_interval_value_t,
    typename t_derivative_filter_t = derivative_filter_t<t_x_t, t_dx_t, t_cutoff_rate_t, t_cutoff_interval_value_t>,
    typename t_signal_filter_t = signal_filter_t<t_x_t, t_cutoff_rate_t, t_cutoff_interval_value_t>>
    requires(is_signed_v<t_x_t> && is_signed_v<t_dx_t> && is_signed_v<t_cutoff_rate_t> && is_signed_v<t_cutoff_slope_t>
        && is_signed_v<t_cutoff_interval_value_t> && !is_fixed_frac<t_cutoff_interval_value_t>)
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

    constexpr explicit filter_t(
        params_t params, derivative_filter_type derivative_filter = {}, signal_filter_type signal_filter = {}) noexcept
        : params_{params}, derivative_filter_{std::move(derivative_filter)}, signal_filter_{std::move(signal_filter)}
    {
        assert((validate<dx_t>(params_).has_value()));
    }

    template <is_fixed dt_ns_t>
        requires(!is_signed_v<dt_ns_t> && !is_fixed_frac<dt_ns_t>)
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

    derivative_filter_type derivative_filter_;
    signal_filter_type signal_filter_;

    bool initialized_{};
};

} // namespace crv::pipeline::filters::one_euro
