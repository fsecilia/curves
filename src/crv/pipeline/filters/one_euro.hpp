// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/pipeline/filters/one_euro/derivative_filter.hpp>
#include <crv/pipeline/filters/one_euro/params.hpp>
#include <crv/pipeline/filters/one_euro/signal_cutoff_rate.hpp>
#include <crv/pipeline/filters/one_euro/signal_filter.hpp>
#include <cassert>
#include <utility>

namespace crv::pipeline::filters::one_euro {

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
        assert(params_.template validate<dx_t>(signal_cutoff_rate_calculator_));
    }

    /// Constructs an initialized filter from complete recursive component state.
    constexpr explicit filter_t(params_t params, derivative_filter_t derivative_filter,
        signal_cutoff_rate_calculator_t signal_cutoff_rate_calculator, signal_filter_t signal_filter) noexcept
        : derivative_filter_{std::move(derivative_filter)},
          signal_cutoff_rate_calculator_{std::move(signal_cutoff_rate_calculator)},
          signal_filter_{std::move(signal_filter)}, params_{params}, initialized_{true}
    {
        assert(params_.template validate<dx_t>(signal_cutoff_rate_calculator_));
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
