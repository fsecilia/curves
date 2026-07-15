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
#include <cstdint>
#include <expected>
#include <type_traits>
#include <utility>

namespace crv::pipeline::filters::one_euro {

namespace detail {

using rne_shifter_t = shifter_t<rounding_modes::shr::nearest_even>;

static constexpr auto rne_shifter = rne_shifter_t{};

} // namespace detail

/// Preconverted runtime parameters for a 1-Euro filter.
///
/// All values use signed storage because the low-pass recurrences use signed saturating FMA operations. Their semantic
/// domains remain nonnegative.
///
/// Runtime units:
///
///     derivative_cutoff_rate  1/ns
///     minimum_cutoff_rate     1/ns
///     cutoff_slope            1/signal-unit
///
/// The filtered derivative uses signal-units/ns, so:
///
///     minimum_cutoff_rate + cutoff_slope*abs(filtered_derivative)
///
/// has units 1/ns.
///
/// Given user-facing continuous-time half-lives in nanoseconds:
///
///     cutoff_rate = ln(2)/half_life_ns
///
/// Given paper-style beta in Hz/(signal-unit/second):
///
///     cutoff_slope = 2*pi*beta
///
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

/// Finite cutoff step or its unrepresentably large limiting case.
///
/// When `saturated` is true, consumers apply the mathematical alpha -> 1 limit instead of evaluating the finite
/// rational recurrence with `value`.
template <is_fixed t_cutoff_step_t>
    requires(is_signed_v<t_cutoff_step_t>)
struct bounded_cutoff_step_t
{
    using cutoff_step_t = t_cutoff_step_t;

    cutoff_step_t value{};
    bool saturated{};
};

/// Narrows a wide, nonnegative cutoff-step product into its working format.
///
/// The working format reserves room for the denominator's `+1`.
///
/// Quantization behavior:
///
///     product == 0 -> zero
///     positive product that rounds to zero -> one raw output unit
///     finite representable product -> nearest-even
///     product above the finite ceiling -> saturated limiting case
///
/// Clamping positive underflow to one raw unit prevents a valid positive cutoff and interval from becoming a frozen
/// filter because of working-type quantization.
template <is_fixed t_cutoff_step_t>
    requires(is_signed_v<t_cutoff_step_t> && !is_fixed_frac<t_cutoff_step_t>)
struct cutoff_step_limiter_t
{
    using cutoff_step_t = t_cutoff_step_t;
    using output_t = bounded_cutoff_step_t<cutoff_step_t>;

    static constexpr auto zero = cutoff_step_t{};
    static constexpr auto one = cutoff_step_t{1};

    /// smallest strictly positive representable cutoff step
    static constexpr auto minimum_positive = cutoff_step_t::literal(1);

    /// largest finite cutoff step for which `value + 1` is representable
    static constexpr auto ceiling = max<cutoff_step_t>() - one;

    template <is_fixed cutoff_step_product_t>
        requires(is_signed_v<cutoff_step_product_t>
            && sizeof(typename cutoff_step_t::value_t) <= sizeof(typename cutoff_step_product_t::value_t)
            && cutoff_step_t::frac_bits <= cutoff_step_product_t::frac_bits
            && cutoff_step_t::int_bits <= cutoff_step_product_t::int_bits)
    constexpr auto operator()(cutoff_step_product_t product) const noexcept -> output_t
    {
        assert(product >= cutoff_step_product_t{});

        // Invalid negative products are still prevented from propagating in release builds. Valid parameters cannot
        // enter this path.
        if (product <= cutoff_step_product_t{}) return {};

        static constexpr auto product_ceiling = cutoff_step_product_t::convert(ceiling);
        if (product > product_ceiling) return {.value = ceiling, .saturated = true};

        auto value = cutoff_step_t::template convert<detail::rne_shifter>(product);
        if (value == zero) value = minimum_positive;

        return {.value = value, .saturated = false};
    }
};

/// Combines the minimum cutoff rate with a wide adaptive cutoff rate.
///
/// Calculates:
///
///     minimum_cutoff_rate + adaptive_cutoff_rate
///
/// in the wide adaptive representation. The result is narrowed once with nearest-even rounding and saturates instead of
/// wrapping.
///
/// Validated parameter sets guarantee that saturation cannot occur for any derivative state in the supported symmetric
/// derivative domain.
template <is_fixed t_cutoff_rate_t>
    requires(is_signed_v<t_cutoff_rate_t>)
struct cutoff_rate_combiner_t
{
    using cutoff_rate_t = t_cutoff_rate_t;

    template <is_fixed adaptive_cutoff_rate_t>
        requires(is_signed_v<adaptive_cutoff_rate_t>
            && sizeof(typename cutoff_rate_t::value_t) < sizeof(typename adaptive_cutoff_rate_t::value_t)
            && cutoff_rate_t::frac_bits <= adaptive_cutoff_rate_t::frac_bits
            && cutoff_rate_t::int_bits <= adaptive_cutoff_rate_t::int_bits)
    static constexpr auto would_saturate(
        cutoff_rate_t minimum_cutoff_rate, adaptive_cutoff_rate_t adaptive_cutoff_rate) noexcept -> bool
    {
        using accumulator_t = fixed_t<typename adaptive_cutoff_rate_t::value_t, adaptive_cutoff_rate_t::frac_bits>;

        auto const adaptive = accumulator_t::convert(adaptive_cutoff_rate);
        auto const minimum = accumulator_t::convert(minimum_cutoff_rate);
        auto const output_max = accumulator_t::convert(max<cutoff_rate_t>());

        return adaptive > output_max - minimum;
    }

    template <is_fixed adaptive_cutoff_rate_t>
        requires(is_signed_v<adaptive_cutoff_rate_t>
            && sizeof(typename cutoff_rate_t::value_t) < sizeof(typename adaptive_cutoff_rate_t::value_t)
            && cutoff_rate_t::frac_bits <= adaptive_cutoff_rate_t::frac_bits
            && cutoff_rate_t::int_bits <= adaptive_cutoff_rate_t::int_bits)
    constexpr auto operator()(
        cutoff_rate_t minimum_cutoff_rate, adaptive_cutoff_rate_t adaptive_cutoff_rate) const noexcept -> cutoff_rate_t
    {
        assert(minimum_cutoff_rate > cutoff_rate_t{});
        assert(adaptive_cutoff_rate >= adaptive_cutoff_rate_t{});

        if (would_saturate(minimum_cutoff_rate, adaptive_cutoff_rate)) { return max<cutoff_rate_t>(); }

        using accumulator_t = fixed_t<typename adaptive_cutoff_rate_t::value_t, adaptive_cutoff_rate_t::frac_bits>;

        auto const combined
            = accumulator_t::convert(minimum_cutoff_rate) + accumulator_t::convert(adaptive_cutoff_rate);

        return cutoff_rate_t::template convert<detail::rne_shifter>(combined);
    }
};

/// Low-pass filter for the signal derivative.
///
/// The paper's derivative leg is:
///
///     raw_derivative = (input - previous_filtered_input)/dt
///     alpha_d = derivative_cutoff_rate*dt/(1 + derivative_cutoff_rate*dt)
///     filtered_derivative = previous_filtered_derivative + alpha_d*(raw_derivative - previous_filtered_derivative)
///
/// Rearranging eliminates the raw derivative division:
///
///     filtered_derivative = (previous_filtered_derivative + derivative_cutoff_rate*(input -previous_filtered_input))
///                           /(1 + derivative_cutoff_rate*dt)
///
/// Only the final division narrows the new derivative state.
///
/// `x_t` uses signed storage for compatibility with the downstream spline, but this pipeline's signal is a nonnegative
/// velocity magnitude. The nonnegative assertions are arithmetic preconditions: they prove that
///
///     input - previous_filtered_input
///
/// fits directly in `x_t`, preserving all of its fractional precision.
///
/// They are not requirements of the mathematical 1-Euro filter.
template <is_fixed t_x_t, is_fixed t_dx_t, is_fixed t_cutoff_rate_t, is_fixed t_cutoff_step_t,
    typename t_cutoff_step_limiter_t = cutoff_step_limiter_t<t_cutoff_step_t>,
    typename t_fma_t = fma_t<fixed::product_t<t_cutoff_rate_t, t_x_t>, t_cutoff_rate_t, t_x_t, t_dx_t,
        detail::rne_shifter_t, fixed::overflow_policy_t::saturate>>
    requires(is_signed_v<t_x_t> && is_signed_v<t_dx_t> && is_signed_v<t_cutoff_rate_t> && is_signed_v<t_cutoff_step_t>
        && !is_fixed_frac<t_cutoff_step_t>)
class derivative_low_pass_t
{
public:
    using x_t = t_x_t;
    using dx_t = t_dx_t;
    using cutoff_rate_t = t_cutoff_rate_t;
    using cutoff_step_t = t_cutoff_step_t;
    using cutoff_step_limiter_t = t_cutoff_step_limiter_t;
    using fma_type = t_fma_t;

    using numerator_t = typename fma_type::out_t;

    static_assert(is_fixed<numerator_t>);
    static_assert(is_signed_v<numerator_t>);

    static_assert(
        numerator_t::int_bits >= dx_t::int_bits, "derivative FMA output must contain the derivative state range");

    static_assert(
        numerator_t::frac_bits >= dx_t::frac_bits, "derivative FMA output must contain the derivative state precision");

    constexpr derivative_low_pass_t() noexcept = default;

    constexpr explicit derivative_low_pass_t(
        dx_t initial, cutoff_step_limiter_t limit_cutoff_step = {}, fma_type fma = {}) noexcept
        : output_{initial}, limit_cutoff_step_{std::move(limit_cutoff_step)}, fma_{std::move(fma)}
    {}

    constexpr auto output() const noexcept -> dx_t { return output_; }

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

        // Because both values are in x_t's nonnegative range:
        //
        //     -max<x_t>()
        //         <= input - previous_filtered_input
        //         <= max<x_t>()
        //
        // The subtraction is therefore representable directly in x_t.
        auto const delta = input - previous_filtered_input;

        auto const cutoff_step_product = multiply(derivative_cutoff_rate, dt_ns);
        auto const cutoff_step = limit_cutoff_step_(cutoff_step_product);

        if (cutoff_step.saturated)
        {
            // As derivative alpha approaches one, the derivative filter approaches the current raw derivative. This is
            // also the correct long-interval behavior because delta/dt approaches zero as dt grows.
            output_ = divide<x_t>(delta, dt_ns, rounding_modes::div::nearest_even);
            return output_;
        }

        auto const numerator = fma_(derivative_cutoff_rate, delta, output_);
        auto const denominator = cutoff_step.value + cutoff_step_t{1};
        output_ = divide<x_t>(numerator, denominator, rounding_modes::div::nearest_even);
        return output_;
    }

private:
    dx_t output_{};
    [[no_unique_address]] cutoff_step_limiter_t limit_cutoff_step_{};
    [[no_unique_address]] fma_type fma_{};
};

/// Calculates the adaptive signal cutoff step.
///
/// Calculates:
///
///     cutoff_rate =
///         minimum_cutoff_rate
///         + cutoff_slope*abs(filtered_derivative)
///
///     cutoff_step = cutoff_rate*dt
///
/// Runtime units:
///
///     minimum_cutoff_rate   1/ns
///     cutoff_slope          1/signal-unit
///     filtered_derivative   signal-unit/ns
///     dt                    ns
///     cutoff_step           dimensionless
///
template <is_fixed t_cutoff_rate_t, is_fixed t_cutoff_slope_t, is_fixed t_cutoff_step_t,
    typename t_cutoff_rate_combiner_t = cutoff_rate_combiner_t<t_cutoff_rate_t>,
    typename t_cutoff_step_limiter_t = cutoff_step_limiter_t<t_cutoff_step_t>>
    requires(is_signed_v<t_cutoff_rate_t> && is_signed_v<t_cutoff_slope_t> && is_signed_v<t_cutoff_step_t>
        && !is_fixed_frac<t_cutoff_step_t>)
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

    template <is_fixed dx_t, is_fixed dt_ns_t>
        requires(is_signed_v<dx_t> && !is_signed_v<dt_ns_t>)
    constexpr auto operator()(cutoff_rate_t minimum_cutoff_rate, cutoff_slope_t cutoff_slope, dx_t filtered_derivative,
        dt_ns_t dt_ns) const noexcept -> output_t
    {
        assert(minimum_cutoff_rate > cutoff_rate_t{});
        assert(cutoff_slope >= cutoff_slope_t{});
        assert(dt_ns > dt_ns_t{});

        auto const adaptive_cutoff_rate = multiply(cutoff_slope, uabs(filtered_derivative));
        auto const combined_cutoff_rate = combine_cutoff_rates_(minimum_cutoff_rate, adaptive_cutoff_rate);
        auto const cutoff_step_product = multiply(combined_cutoff_rate, dt_ns);

        return limit_cutoff_step_(cutoff_step_product);
    }

private:
    [[no_unique_address]] cutoff_rate_combiner_t combine_cutoff_rates_{};
    [[no_unique_address]] cutoff_step_limiter_t limit_cutoff_step_{};
};

/// Low-pass filter for the input signal.
///
/// The ordinary recurrence is:
///
///     alpha = cutoff_step/(1 + cutoff_step)
///     filtered = previous_filtered + alpha*(input - previous_filtered)
///
/// Rearranging gives:
///
///     filtered = (previous_filtered + cutoff_step*input)/(1 + cutoff_step)
///
/// This avoids materializing alpha and avoids narrowing an EMA correction. With the default exact product FMA output,
/// only the final division narrows the new signal state.
///
/// The nonnegative assertions preserve the velocity-magnitude invariant relied upon by derivative_low_pass_t.
template <is_fixed t_x_t, is_fixed t_cutoff_step_t,
    typename t_fma_t = fma_t<fixed::product_t<t_cutoff_step_t, t_x_t>, t_cutoff_step_t, t_x_t, t_x_t,
        detail::rne_shifter_t, fixed::overflow_policy_t::saturate>>
    requires(is_signed_v<t_x_t> && is_signed_v<t_cutoff_step_t> && !is_fixed_frac<t_cutoff_step_t>)
class signal_low_pass_t
{
public:
    using x_t = t_x_t;
    using cutoff_step_t = t_cutoff_step_t;
    using bounded_step_t = bounded_cutoff_step_t<cutoff_step_t>;

    using fma_type = t_fma_t;
    using numerator_t = typename fma_type::out_t;

    static_assert(is_fixed<numerator_t>);
    static_assert(is_signed_v<numerator_t>);
    static_assert(numerator_t::int_bits >= x_t::int_bits, "signal FMA output must contain the signal state range");

    static_assert(
        numerator_t::frac_bits >= x_t::frac_bits, "signal FMA output must contain the signal state precision");

    static constexpr auto cutoff_step_ceiling = max<cutoff_step_t>() - cutoff_step_t{1};

    constexpr signal_low_pass_t() noexcept = default;

    constexpr explicit signal_low_pass_t(x_t initial, fma_type fma = {}) noexcept
        : output_{initial}, fma_{std::move(fma)}
    {}

    constexpr auto output() const noexcept -> x_t { return output_; }
    constexpr void reset(x_t initial = {}) noexcept { output_ = initial; }

    constexpr auto operator()(x_t input, bounded_step_t cutoff_step) noexcept -> x_t
    {
        assert(input >= x_t{});
        assert(output_ >= x_t{});
        assert(cutoff_step.value >= cutoff_step_t{});
        assert(cutoff_step.saturated || cutoff_step.value <= cutoff_step_ceiling);

        if (cutoff_step.saturated)
        {
            // alpha -> 1
            output_ = input;
            return output_;
        }

        auto const numerator = fma_(cutoff_step.value, input, output_);
        auto const denominator = cutoff_step.value + cutoff_step_t{1};
        output_ = divide<x_t>(numerator, denominator, rounding_modes::div::nearest_even);
        return output_;
    }

private:
    x_t output_{};

    [[no_unique_address]]
    fma_type fma_{};
};

enum class validation_error
{
    derivative_cutoff_rate_not_positive,
    minimum_cutoff_rate_not_positive,
    cutoff_slope_negative,
    adaptive_cutoff_rate_overflow,
};

/// Validates runtime 1-Euro parameters independently of a timing envelope.
///
/// This validator deliberately does not require a minimum event interval. Cutoff-step underflow and overflow have
/// defined runtime behavior:
///
///     positive underflow
///         -> minimum positive working cutoff step
///
///     overflow
///         -> alpha -> 1 limiting behavior
///
/// The adaptive-cutoff check covers every representable derivative state:
///
///     [min<dx_t>(), max<dx_t>()]
///
/// Since two's-complement has an asymmetric range, the largest magnitude is:
///
///     uabs(min<dx_t>())
///
template <is_fixed t_dx_t, is_fixed t_cutoff_rate_t, is_fixed t_cutoff_slope_t,
    typename t_cutoff_rate_combiner_t = cutoff_rate_combiner_t<t_cutoff_rate_t>>
    requires(is_signed_v<t_dx_t> && is_signed_v<t_cutoff_rate_t> && is_signed_v<t_cutoff_slope_t>)
struct parameter_validator_t
{
    using dx_t = t_dx_t;
    using cutoff_rate_t = t_cutoff_rate_t;
    using cutoff_slope_t = t_cutoff_slope_t;
    using cutoff_rate_combiner_t = t_cutoff_rate_combiner_t;

    using params_t = parameters_t<cutoff_rate_t, cutoff_slope_t>;

    constexpr auto operator()(params_t const& params) const noexcept -> std::expected<void, validation_error>
    {
        if (params.derivative_cutoff_rate <= cutoff_rate_t{})
        {
            return std::unexpected{validation_error::derivative_cutoff_rate_not_positive};
        }

        if (params.minimum_cutoff_rate <= cutoff_rate_t{})
        {
            return std::unexpected{validation_error::minimum_cutoff_rate_not_positive};
        }

        if (params.cutoff_slope < cutoff_slope_t{}) { return std::unexpected{validation_error::cutoff_slope_negative}; }

        auto const maximum_derivative_magnitude = uabs(min<dx_t>());
        auto const maximum_adaptive_cutoff_rate = multiply(params.cutoff_slope, maximum_derivative_magnitude);

        using accumulator_t = std::remove_cvref_t<decltype(maximum_adaptive_cutoff_rate)>;

        static_assert(is_fixed<accumulator_t>);
        static_assert(is_signed_v<accumulator_t>);
        static_assert(sizeof(typename cutoff_rate_t::value_t) < sizeof(typename accumulator_t::value_t));
        static_assert(cutoff_rate_t::frac_bits <= accumulator_t::frac_bits);
        static_assert(cutoff_rate_t::int_bits <= accumulator_t::int_bits);

        if (cutoff_rate_combiner_t::would_saturate(params.minimum_cutoff_rate, maximum_adaptive_cutoff_rate))
        {
            return std::unexpected{validation_error::adaptive_cutoff_rate_overflow};
        }

        return {};
    }
};

/// Variable-interval fixed-point 1-Euro filter.
///
/// Implements Casiez, Roussel, and Vogel, “1 € Filter: A Simple Speed-based Low-pass Filter for Noisy Input in
/// Interactive Systems,” CHI 2012.
///
/// This project filters a nonnegative velocity magnitude stored in a signed fixed-point type for compatibility with the
/// downstream spline evaluator.
///
/// For each sample after initialization:
///
///     previous_filtered_input = signal_low_pass.output()
///     filtered_derivative = derivative_low_pass(input, previous_filtered_input, derivative_cutoff_rate, dt_ns)
///     signal_cutoff_step = cutoff_step_calculator(minimum_cutoff_rate, cutoff_slope, filtered_derivative, dt_ns)
///     filtered_input = signal_low_pass(input, signal_cutoff_step)
///
/// The first sample seeds the signal state and clears the derivative state. No previous raw input is stored.
template <is_fixed t_x_t, is_fixed t_dx_t, is_fixed t_cutoff_rate_t, is_fixed t_cutoff_slope_t,
    is_fixed t_cutoff_step_t,
    typename t_derivative_low_pass_t = derivative_low_pass_t<t_x_t, t_dx_t, t_cutoff_rate_t, t_cutoff_step_t>,
    typename t_cutoff_step_calculator_t = cutoff_step_calculator_t<t_cutoff_rate_t, t_cutoff_slope_t, t_cutoff_step_t>,
    typename t_signal_low_pass_t = signal_low_pass_t<t_x_t, t_cutoff_step_t>>
    requires(is_signed_v<t_x_t> && is_signed_v<t_dx_t> && is_signed_v<t_cutoff_rate_t> && is_signed_v<t_cutoff_slope_t>
        && is_signed_v<t_cutoff_step_t> && !is_fixed_frac<t_cutoff_step_t>)
class filter_t
{
public:
    using x_t = t_x_t;
    using dx_t = t_dx_t;
    using cutoff_rate_t = t_cutoff_rate_t;
    using cutoff_slope_t = t_cutoff_slope_t;
    using cutoff_step_t = t_cutoff_step_t;
    using derivative_low_pass_type = t_derivative_low_pass_t;
    using cutoff_step_calculator_type = t_cutoff_step_calculator_t;
    using signal_low_pass_type = t_signal_low_pass_t;

    using params_t = parameters_t<cutoff_rate_t, cutoff_slope_t>;

    constexpr explicit filter_t(params_t params) noexcept : params_{params}
    {
        assert((parameter_validator_t<dx_t, cutoff_rate_t, cutoff_slope_t>{}(params_).has_value()));
    }

    /// Dependency/state injection constructor.
    ///
    /// When initialized is true, the injected low-pass objects must contain the complete current recursive state.
    constexpr explicit filter_t(params_t params, derivative_low_pass_type derivative_low_pass,
        cutoff_step_calculator_type cutoff_step_calculator, signal_low_pass_type signal_low_pass,
        bool initialized) noexcept
        : params_{params}, derivative_low_pass_{std::move(derivative_low_pass)},
          cutoff_step_calculator_{std::move(cutoff_step_calculator)}, signal_low_pass_{std::move(signal_low_pass)},
          initialized_{initialized}
    {}

    /// Discards all filter history.
    ///
    /// The next input seeds the signal state and clears the derivative state.
    constexpr void reset() noexcept
    {
        derivative_low_pass_.reset();
        signal_low_pass_.reset();
        initialized_ = false;
    }

    /// Filters one input sample.
    ///
    /// dt_ns is ignored for the first sample after construction or reset.
    template <is_fixed dt_ns_t>
        requires(!is_signed_v<dt_ns_t>)
    constexpr auto operator()(x_t input, dt_ns_t dt_ns) noexcept -> x_t
    {
        // This project filters velocity magnitude. Besides expressing the signal's physical domain, this invariant
        // allows the derivative component to subtract two x_t values without sacrificing fractional precision for a
        // wider difference representation.
        assert(input >= x_t{});

        if (!initialized_) [[unlikely]]
        {
            derivative_low_pass_.reset(dx_t{});
            signal_low_pass_.reset(input);
            initialized_ = true;
            return input;
        }

        auto const previous_filtered_input = signal_low_pass_.output();
        auto const filtered_derivative
            = derivative_low_pass_(input, previous_filtered_input, params_.derivative_cutoff_rate, dt_ns);
        auto const signal_cutoff_step
            = cutoff_step_calculator_(params_.minimum_cutoff_rate, params_.cutoff_slope, filtered_derivative, dt_ns);

        return signal_low_pass_(input, signal_cutoff_step);
    }

    constexpr auto signal_state() const noexcept -> x_t { return signal_low_pass_.output(); }
    constexpr auto derivative_state() const noexcept -> dx_t { return derivative_low_pass_.output(); }

    constexpr auto initialized() const noexcept -> bool { return initialized_; }

private:
    params_t params_;
    derivative_low_pass_type derivative_low_pass_{};
    [[no_unique_address]] cutoff_step_calculator_type cutoff_step_calculator_{};
    signal_low_pass_type signal_low_pass_{};
    bool initialized_{};
};

} // namespace crv::pipeline::filters::one_euro
