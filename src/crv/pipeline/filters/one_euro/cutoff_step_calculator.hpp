// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/uabs.hpp>
#include <utility>

namespace crv::pipeline::filters::one_euro {

/// calcs cutoff_step = clamp((omega_min + beta*abs(filtered_dx))*dt_ns).
template <is_fixed t_cutoff_step_t, typename t_cutoff_rate_combiner_t, typename t_cutoff_step_clamp_t>
class cutoff_step_calculator_t
{
public:
    using cutoff_step_t = t_cutoff_step_t;
    using cutoff_rate_t = cutoff_step_t; // same representation, but physical units differ
    using cutoff_rate_combiner_t = t_cutoff_rate_combiner_t;
    using cutoff_step_clamp_t = t_cutoff_step_clamp_t;

    constexpr cutoff_step_calculator_t() noexcept = default;

    constexpr explicit cutoff_step_calculator_t(
        cutoff_rate_combiner_t combine_cutoff_rates, cutoff_step_clamp_t clamp_cutoff_step = {}) noexcept
        : combine_cutoff_rates_{std::move(combine_cutoff_rates)}, clamp_cutoff_step_{std::move(clamp_cutoff_step)}
    {}

    template <is_fixed dx_t, is_fixed dt_ns_fixed_t>
    constexpr auto operator()(cutoff_rate_t omega_min, cutoff_rate_t beta, dx_t filtered_dx,
        dt_ns_fixed_t dt_ns) const noexcept -> cutoff_step_t
    {
        auto const adaptive_cutoff_rate = multiply(beta, uabs(filtered_dx));
        auto const combined_cutoff_rate = combine_cutoff_rates_(omega_min, adaptive_cutoff_rate);
        return clamp_cutoff_step_(multiply(combined_cutoff_rate, dt_ns));
    }

private:
    [[no_unique_address]] cutoff_rate_combiner_t combine_cutoff_rates_{};
    [[no_unique_address]] cutoff_step_clamp_t clamp_cutoff_step_{};
};

} // namespace crv::pipeline::filters::one_euro
