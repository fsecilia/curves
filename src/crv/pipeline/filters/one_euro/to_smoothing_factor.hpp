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

/// calcs smoothing factor from cutoff step
///
/// \tparam smoothing_factor_t smoothing factor output, unsigned fractional in [0, 1)
template <is_fixed smoothing_factor_t>
    requires(!is_signed_v<typename smoothing_factor_t::value_t>
        && smoothing_factor_t::frac_bits == sizeof(typename smoothing_factor_t::value_t) * CHAR_BIT)
struct to_smoothing_factor_t
{
    /// calcs `cutoff_step/(1 + cutoff_step)` in `[0, 1)`
    ///
    /// When `cutoff_step` is large enough that `cutoff_step + 1` would overflow cutoff_step_t, returns `~= 1`. Since
    /// the output is an unsigned fractional type with no integer bits, that result becomes `max<smoothing_factor_t>()`.
    ///
    /// \tparam cutoff_step_t unsigned fixed; must have at least one integer bit to represent 1.0
    /// \param cutoff_step cutoff step input
    /// \returns cutoff_step/(1 + cutoff_step), or 1 - epsilon on overflow
    template <is_fixed cutoff_step_t>
    constexpr auto operator()(cutoff_step_t cutoff_step) const noexcept -> smoothing_factor_t
        requires(!crv::is_signed_v<typename cutoff_step_t::value_t>
            && cutoff_step_t::frac_bits < sizeof(typename cutoff_step_t::value_t) * CHAR_BIT)
    {
        static constexpr auto one = cutoff_step_t{1};
        if (cutoff_step > max<cutoff_step_t>() - one) return max<smoothing_factor_t>();

        // divide with truncate to maintain half-open range
        //
        // Using rne here would require saturation to prevent rounding ~= 1 up, wrapping to 0. In prod, the quantization
        // error from truncating is 2^-65, so just truncate.
        return divide<smoothing_factor_t>(cutoff_step, cutoff_step + one);
    }
};

} // namespace crv::pipeline::filters::one_euro
