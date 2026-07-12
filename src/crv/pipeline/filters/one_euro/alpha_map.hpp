// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/shifter.hpp>
#include <cassert>

namespace crv::pipeline::filters::one_euro {

/// maps dimensionless cutoff step to alpha = cutoff_step/(1 + cutoff_step).
template <is_fixed_frac smoothing_factor_t>
    requires(!is_signed_v<smoothing_factor_t>)
struct alpha_map_t
{
    template <is_fixed cutoff_step_t>
    constexpr auto operator()(cutoff_step_t cutoff_step) const noexcept -> smoothing_factor_t
        requires(!is_signed_v<cutoff_step_t> && !is_fixed_frac<cutoff_step_t>)
    {
        static constexpr auto one = cutoff_step_t{1};
        if (cutoff_step > max<cutoff_step_t>() - one) return max<smoothing_factor_t>();

        // truncate here instead of rne to preserve half-open output range [0, 1)
        return divide<smoothing_factor_t>(cutoff_step, cutoff_step + one);
    }
};

} // namespace crv::pipeline::filters::one_euro
