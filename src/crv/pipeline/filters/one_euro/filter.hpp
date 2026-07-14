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

/// exponential moving average low-pass smoother over signed fixed-point signals
///
/// Applies y[n] = y[n - 1] + alpha[n] * (x[n] - y[n - 1]) with alpha supplied by the caller.
template <is_fixed sample_t, shifter_t shifter = {}>
    requires(is_signed_v<sample_t>)
class ema_accumulator_t
{
public:
    constexpr ema_accumulator_t() noexcept = default;
    constexpr explicit ema_accumulator_t(sample_t initial) noexcept : output_{initial} {}

    constexpr auto output() const noexcept -> sample_t { return output_; }

    constexpr void reset(sample_t initial = {}) noexcept { output_ = initial; }

    template <is_fixed_frac smoothing_factor_t>
        requires(!is_signed_v<smoothing_factor_t>)
    constexpr auto operator()(sample_t input, smoothing_factor_t alpha) noexcept -> sample_t
    {
        auto const error = saturating_sub(input, output_);
        auto const correction = multiply<sample_t, shifter>(alpha, error);

        // This sum cannot overflow. correction has error's sign and cannot round past it, so the sum remains between
        // the previous output and the input.
        output_ += correction;

        return output_;
    }

private:
    sample_t output_{};
};

/// maps dimensionless cutoff-step product to alpha = cutoff_step / (1 + cutoff_step).
///
/// The input uses a wide product representation. It is narrowed with nearest-even rounding into cutoff_step_t, whose
/// upper bound reserves room for the denominator's +1. Larger products saturate at that bound, producing a continuous
/// alpha plateau strictly below one.
template <is_fixed t_cutoff_step_t, is_fixed_frac t_smoothing_factor_t>
    requires(!is_signed_v<t_cutoff_step_t> && !is_fixed_frac<t_cutoff_step_t> && !is_signed_v<t_smoothing_factor_t>)
struct alpha_map_t
{
    using cutoff_step_t = t_cutoff_step_t;
    using smoothing_factor_t = t_smoothing_factor_t;

    static constexpr auto cutoff_step_one = cutoff_step_t{1};

    /// largest working cutoff step for which cutoff_step + 1 remains representable
    static constexpr auto cutoff_step_ceiling = max<cutoff_step_t>() - cutoff_step_one;

    template <is_fixed cutoff_step_product_t>
        requires(!is_signed_v<cutoff_step_product_t>
            && sizeof(typename cutoff_step_t::value_t) <= sizeof(typename cutoff_step_product_t::value_t)
            && cutoff_step_t::frac_bits <= cutoff_step_product_t::frac_bits
            && cutoff_step_t::int_bits <= cutoff_step_product_t::int_bits)
    constexpr auto operator()(cutoff_step_product_t cutoff_step_product) const noexcept -> smoothing_factor_t
    {
        static constexpr auto product_ceiling = cutoff_step_product_t::convert(cutoff_step_ceiling);

        static constexpr auto rne_shifter = shifter_t<rounding_modes::shr::nearest_even>{};
        auto const cutoff_step = cutoff_step_product > product_ceiling
            ? cutoff_step_ceiling
            : cutoff_step_t::template convert<rne_shifter>(cutoff_step_product);

        // truncate instead of using RNE so the output remains in the half-open range [0, 1)
        return divide<smoothing_factor_t>(cutoff_step, cutoff_step + cutoff_step_one);
    }
};

/// combines omega_min and beta*abs(filtered_dx) without allowing the aligned sum to wrap
template <is_fixed cutoff_rate_t>
    requires(!is_signed_v<cutoff_rate_t>)
struct cutoff_rate_combiner_t
{
    template <is_fixed adaptive_cutoff_rate_t>
        requires(!is_signed_v<adaptive_cutoff_rate_t>
            && sizeof(typename cutoff_rate_t::value_t) < sizeof(typename adaptive_cutoff_rate_t::value_t)
            && cutoff_rate_t::frac_bits < adaptive_cutoff_rate_t::frac_bits
            && cutoff_rate_t::int_bits <= adaptive_cutoff_rate_t::int_bits)
    constexpr auto operator()(cutoff_rate_t omega_min, adaptive_cutoff_rate_t adaptive_cutoff_rate) const noexcept
        -> cutoff_rate_t
    {
        using accumulator_t = fixed_t<typename adaptive_cutoff_rate_t::value_t, adaptive_cutoff_rate_t::frac_bits>;
        auto const adaptive_aligned = accumulator_t::convert(adaptive_cutoff_rate);
        auto const minimum_aligned = accumulator_t::convert(omega_min);
        auto const output_max = accumulator_t::convert(max<cutoff_rate_t>());

        static constexpr auto rne_shifter = shifter_t<rounding_modes::shr::nearest_even>{};
        return (adaptive_aligned > output_max - minimum_aligned)
            ? max<cutoff_rate_t>()
            : cutoff_rate_t::template convert<rne_shifter>(adaptive_aligned + minimum_aligned);
    }
};

/// Calculates the wide, dimensionless cutoff-step product
/// (omega_min + beta * abs(filtered_dx)) * dt_ns.
template <is_fixed t_cutoff_rate_t, typename t_cutoff_rate_combiner_t>
    requires(!is_signed_v<t_cutoff_rate_t>)
class cutoff_step_calculator_t
{
public:
    using cutoff_rate_t = t_cutoff_rate_t;
    using cutoff_rate_combiner_t = t_cutoff_rate_combiner_t;

    constexpr cutoff_step_calculator_t() noexcept = default;

    constexpr explicit cutoff_step_calculator_t(cutoff_rate_combiner_t combine_cutoff_rates) noexcept
        : combine_cutoff_rates_{std::move(combine_cutoff_rates)}
    {}

    template <is_fixed dx_t, is_fixed dt_ns_fixed_t>
        requires(!is_signed_v<dt_ns_fixed_t>)
    constexpr auto operator()(
        cutoff_rate_t omega_min, cutoff_rate_t beta, dx_t filtered_dx, dt_ns_fixed_t dt_ns) const noexcept
    {
        auto const adaptive_cutoff_rate = multiply(beta, uabs(filtered_dx));
        auto const combined_cutoff_rate = combine_cutoff_rates_(omega_min, adaptive_cutoff_rate);
        return multiply(combined_cutoff_rate, dt_ns);
    }

private:
    [[no_unique_address]] cutoff_rate_combiner_t combine_cutoff_rates_{};
};

/// estimates and low-pass filters the derivative of a fixed-point signal
template <is_fixed t_x_t, is_fixed t_dx_t, typename t_ema_accumulator_t>
    requires(is_signed_v<t_x_t> && is_signed_v<t_dx_t>)
class derivative_estimator_t
{
public:
    using x_t = t_x_t;
    using dx_t = t_dx_t;
    using ema_accumulator_t = t_ema_accumulator_t;

    constexpr derivative_estimator_t() noexcept = default;

    constexpr explicit derivative_estimator_t(ema_accumulator_t ema_accumulator, x_t previous = {}) noexcept
        : ema_accumulator_{std::move(ema_accumulator)}, prev_{previous}
    {}

    constexpr auto output() const noexcept -> dx_t { return ema_accumulator_.output(); }
    constexpr auto prev() const noexcept -> x_t { return prev_; }

    /// seeds previous raw sample and clears the filtered derivative.
    constexpr void reset(x_t prev = {}) noexcept
    {
        prev_ = prev;
        ema_accumulator_.reset(dx_t{});
    }

    template <is_fixed reciprocal_dt_ms_t, is_fixed_frac smoothing_factor_t>
        requires(!is_signed_v<reciprocal_dt_ms_t> && !is_signed_v<smoothing_factor_t>)
    constexpr auto operator()(x_t x, reciprocal_dt_ms_t reciprocal_dt_ms, smoothing_factor_t alpha) noexcept -> dx_t
    {
        auto const delta = saturating_sub(x, prev_);
        prev_ = x;

        static constexpr auto rne_shifter = shifter_t<rounding_modes::shr::nearest_even>{};
        auto const derivative = multiply<dx_t, rne_shifter>(delta, reciprocal_dt_ms);
        return ema_accumulator_(derivative, alpha);
    }

private:
    ema_accumulator_t ema_accumulator_{};
    x_t prev_{};
};

/// 1-euro filter over fixed-point signals
///
/// This type implements Casiez, Roussel, and Vogel, “1 € Filter: A Simple Speed-based Low-pass Filter for Noisy Input
/// in Interactive Systems,” CHI 2012, https://doi.org/10.1145/2207676.2208639.
///
/// Names and conventions follow the paper. Rates stored in params_t have 2*pi and the ns conversion folded in:
///
///     derivative_alpha = alpha_map(omega_derivative * dt_ns)
///     filtered_dx = low_pass((x - previous_x) * reciprocal_dt_ms, derivative_alpha)
///     signal_alpha = alpha_map((omega_min + beta * abs(filtered_dx)) * dt_ns)
///     filtered_x = low_pass(x, signal_alpha)
///
/// The first sample seeds the signal state and previous raw sample and clears the derivative state. Later samples use
/// the ordinary recurrence. Both alpha legs use the same continuous, overflow-safe mapping.
///
/// reciprocal_dt_ms and dt_ns are both supplied to avoid another division. After initialization,
/// reciprocal_dt_ms must be the nearest representable reciprocal of dt_ns. For a sufficiently long interval that
/// reciprocal may legitimately round to zero.
template <is_fixed t_x_t, is_fixed t_dx_t, is_fixed t_cutoff_rate_t = fixed_t<uint64_t, 64>,
    is_fixed t_alpha_cutoff_step_t = fixed_t<uint64_t, 44>, is_fixed_frac t_smoothing_factor_t = fixed_t<uint64_t, 64>,
    typename t_derivative_alpha_map_t = alpha_map_t<t_alpha_cutoff_step_t, t_smoothing_factor_t>,
    typename t_derivative_estimator_t = derivative_estimator_t<t_x_t, t_dx_t, ema_accumulator_t<t_dx_t>>,
    typename t_cutoff_step_calculator_t
    = cutoff_step_calculator_t<t_cutoff_rate_t, cutoff_rate_combiner_t<t_cutoff_rate_t>>,
    typename t_signal_alpha_map_t = alpha_map_t<t_alpha_cutoff_step_t, t_smoothing_factor_t>,
    typename t_signal_ema_t = ema_accumulator_t<t_x_t>>
    requires(is_signed_v<t_x_t> && is_signed_v<t_dx_t> && !is_signed_v<t_cutoff_rate_t>
        && !is_signed_v<t_alpha_cutoff_step_t> && !is_fixed_frac<t_alpha_cutoff_step_t>
        && !is_signed_v<t_smoothing_factor_t>)
class filter_t
{
public:
    using x_t = t_x_t;
    using dx_t = t_dx_t;
    using cutoff_rate_t = t_cutoff_rate_t;
    using alpha_cutoff_step_t = t_alpha_cutoff_step_t;
    using smoothing_factor_t = t_smoothing_factor_t;
    using dt_ns_t = uint64_t;
    using dt_ns_fixed_t = fixed_t<uint64_t, 0>;

    using derivative_alpha_map_t = t_derivative_alpha_map_t;
    using derivative_estimator_t = t_derivative_estimator_t;
    using cutoff_step_calculator_t = t_cutoff_step_calculator_t;
    using signal_alpha_map_t = t_signal_alpha_map_t;
    using signal_ema_t = t_signal_ema_t;

    struct params_t
    {
        cutoff_rate_t omega_derivative; ///< 2*pi*f_c_derivative*1e-9 [rad/ns]
        cutoff_rate_t omega_min; ///< 2*pi*f_c_min*1e-9 [rad/ns]
        cutoff_rate_t beta; ///< 2*pi*beta*1e-9 [rad/ns per unit abs(filtered_dx)]
    };

    constexpr explicit filter_t(params_t params) noexcept : params_{params} {}

    constexpr explicit filter_t(params_t params, derivative_alpha_map_t derivative_alpha_map,
        derivative_estimator_t derivative_estimator, cutoff_step_calculator_t cutoff_step_calculator,
        signal_alpha_map_t signal_alpha_map, signal_ema_t signal_ema) noexcept
        : params_{params}, derivative_alpha_map_{std::move(derivative_alpha_map)},
          derivative_estimator_{std::move(derivative_estimator)},
          cutoff_step_calculator_{std::move(cutoff_step_calculator)}, signal_alpha_map_{std::move(signal_alpha_map)},
          signal_ema_{std::move(signal_ema)}
    {}

    /// discards all history
    ///
    /// The next sample is treated as the first sample of a new stream.
    constexpr void reset() noexcept
    {
        derivative_estimator_.reset();
        signal_ema_.reset();
        initialized_ = false;
    }

    /// filters one sample
    ///
    /// reciprocal_dt_ms and dt_ns are ignored for the first sample after construction or reset.
    ///
    /// \param reciprocal_dt_ms nearest representable reciprocal interval in 1/ms; may be zero after a long idle
    /// \param dt_ns elapsed interval in ns; must be nonzero after initialization
    template <is_fixed reciprocal_dt_ms_t>
        requires(!is_signed_v<reciprocal_dt_ms_t>)
    constexpr auto operator()(x_t x, reciprocal_dt_ms_t reciprocal_dt_ms, dt_ns_t dt_ns) noexcept -> x_t
    {
        if (!initialized_) [[unlikely]]
        {
            derivative_estimator_.reset(x);
            signal_ema_.reset(x);
            initialized_ = true;
            return x;
        }

        assert(dt_ns != 0 && "one_euro: zero dt_ns");
        assert(reciprocal_dt_consistent(reciprocal_dt_ms, dt_ns) && "one_euro: reciprocal_dt_ms does not match dt_ns");

        auto const dt_ns_fixed = dt_ns_fixed_t::literal(dt_ns);

        auto const derivative_cutoff_step_product = multiply(params_.omega_derivative, dt_ns_fixed);
        auto const derivative_alpha = derivative_alpha_map_(derivative_cutoff_step_product);
        auto const filtered_dx = derivative_estimator_(x, reciprocal_dt_ms, derivative_alpha);

        auto const signal_cutoff_step_product
            = cutoff_step_calculator_(params_.omega_min, params_.beta, filtered_dx, dt_ns_fixed);
        auto const signal_alpha = signal_alpha_map_(signal_cutoff_step_product);
        return signal_ema_(x, signal_alpha);
    }

    constexpr auto signal_state() const noexcept -> x_t { return signal_ema_.output(); }
    constexpr auto derivative_state() const noexcept -> dx_t { return derivative_estimator_.output(); }

private:
    template <is_fixed reciprocal_dt_ms_t>
    static constexpr auto reciprocal_dt_consistent(reciprocal_dt_ms_t reciprocal_dt_ms, dt_ns_t dt_ns) noexcept -> bool
    {
        auto const product = multiply(reciprocal_dt_ms, dt_ns_fixed_t::literal(dt_ns));
        using rvalue_t = typename reciprocal_dt_ms_t::value_t;
        using product_value_t = typename decltype(product)::value_t;

        static_assert(sizeof(product_value_t) >= sizeof(rvalue_t) + sizeof(dt_ns_t),
            "reciprocal consistency product must be wide enough not to wrap");

        auto const target = product_value_t{1'000'000} << reciprocal_dt_ms_t::frac_bits;
        auto const diff = product.value > target ? product.value - target : target - product.value;
        return diff <= product_value_t{dt_ns} / 2 + 1;
    }

    params_t params_;
    [[no_unique_address]] derivative_alpha_map_t derivative_alpha_map_{};
    derivative_estimator_t derivative_estimator_{};
    [[no_unique_address]] cutoff_step_calculator_t cutoff_step_calculator_{};
    [[no_unique_address]] signal_alpha_map_t signal_alpha_map_{};
    signal_ema_t signal_ema_{};
    bool initialized_{};
};

} // namespace crv::pipeline::filters::one_euro
