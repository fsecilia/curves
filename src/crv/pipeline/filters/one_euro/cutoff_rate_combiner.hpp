// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/rounding_mode.hpp>
#include <crv/math/shifter.hpp>

namespace crv::pipeline::filters::one_euro {

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

} // namespace crv::pipeline::filters::one_euro
