// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/uabs.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/rounding_mode.hpp>
#include <crv/math/shifter.hpp>
#include <crv/pipeline/filters/one_euro/alpha_map.hpp>
#include <crv/pipeline/filters/one_euro/cutoff_rate_combiner.hpp>
#include <crv/pipeline/filters/one_euro/cutoff_step_calculator.hpp>
#include <crv/pipeline/filters/one_euro/cutoff_step_clamp.hpp>
#include <crv/pipeline/filters/one_euro/derivative_estimator.hpp>
#include <crv/pipeline/filters/one_euro/ema_accumulator.hpp>
#include <cassert>
#include <utility>

namespace crv::pipeline::filters::one_euro {

/// Implements Casiez, Roussel, and Vogel, “1 € Filter: A Simple Speed-based Low-pass Filter for Noisy Input in
/// Interactive Systems,” CHI 2012, https://doi.org/10.1145/2207676.2208639.
///
/// Names and conventions follow the paper. Rates stored in params_t have 2*pi and the ns conversion folded in:
///
///     derivative_alpha = alpha_map(omega_derivative*dt_ns)
///     filtered_dx = low_pass((x - previous_x)*reciprocal_dt_ms, derivative_alpha)
///     signal_alpha = alpha_map((omega_min + beta*abs(filtered_dx))*dt_ns)
///     filtered_x = low_pass(x, signal_alpha)
///
/// The derivative cutoff step is intentionally not clamped to 31. After a long idle, reciprocal_dt_ms can round to
/// zero while derivative alpha reaches effective passthrough, clearing stale derivative state in one sample. The
/// signal cutoff step remains clamped so its alpha is at most 31/32.
///
/// reciprocal_dt_ms and dt_ns are both supplied to avoid another division. reciprocal_dt_ms must be the nearest
/// representable reciprocal of dt_ns. For a sufficiently long interval that value is legitimately zero; the debug
/// consistency contract accepts that idle-reset regime.
template <is_fixed t_x_t, is_fixed t_dx_t, is_fixed t_cutoff_step_t = fixed_t<uint64_t, 58>,
    is_fixed t_smoothing_factor_t = fixed_t<uint64_t, 64>,
    typename t_derivative_alpha_map_t = alpha_map_t<t_smoothing_factor_t>,
    typename t_derivative_estimator_t = derivative_estimator_t<t_x_t, t_dx_t, ema_accumulator_t<t_dx_t>>,
    typename t_cutoff_step_calculator_t = cutoff_step_calculator_t<t_cutoff_step_t,
        cutoff_rate_combiner_t<t_cutoff_step_t>, cutoff_step_clamp_t<t_cutoff_step_t>>,
    typename t_signal_alpha_map_t = alpha_map_t<t_smoothing_factor_t>,
    typename t_signal_ema_t = ema_accumulator_t<t_x_t>>
class filter_t
{
public:
    using x_t = t_x_t;
    using dx_t = t_dx_t;
    using cutoff_step_t = t_cutoff_step_t;
    using cutoff_rate_t = cutoff_step_t; // same representation, but physical units differ
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

    /// filters one sample
    ///
    /// \param reciprocal_dt_ms nearest representable reciprocal interval in 1/ms; may be zero after a long idle
    /// \param dt_ns elapsed interval in ns; must be nonzero
    template <is_fixed reciprocal_dt_ms_t>
    constexpr auto operator()(x_t x, reciprocal_dt_ms_t reciprocal_dt_ms, dt_ns_t dt_ns) noexcept -> x_t
    {
        assert(dt_ns != 0 && "one_euro: zero dt_ns");
        assert(reciprocal_dt_consistent(reciprocal_dt_ms, dt_ns) && "one_euro: reciprocal_dt_ms does not match dt_ns");

        auto const dt_ns_fixed = dt_ns_fixed_t::literal(dt_ns);

        auto const derivative_cutoff_step = cutoff_step_t::convert(multiply(params_.omega_derivative, dt_ns_fixed));
        auto const derivative_alpha = derivative_alpha_map_(derivative_cutoff_step);
        auto const filtered_dx = derivative_estimator_(x, reciprocal_dt_ms, derivative_alpha);

        auto const signal_cutoff_step
            = cutoff_step_calculator_(params_.omega_min, params_.beta, filtered_dx, dt_ns_fixed);
        auto const signal_alpha = signal_alpha_map_(signal_cutoff_step);
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
};

} // namespace crv::pipeline::filters::one_euro
