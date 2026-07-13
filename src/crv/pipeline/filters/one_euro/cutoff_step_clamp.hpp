// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/rounding_mode.hpp>
#include <crv/math/shifter.hpp>
#include <cassert>

namespace crv::pipeline::filters::one_euro {

/// clamps dimensionless, per-sample cutoff step to fit cutoff_step_t
template <is_fixed cutoff_step_t>
    requires(!is_signed_v<cutoff_step_t>)
struct cutoff_step_clamp_t
{
    static constexpr auto cutoff_ceiling = []() {
        using cutoff_step_value_t = cutoff_step_t::value_t;
        return cutoff_step_t{(cutoff_step_value_t{1} << (cutoff_step_t::int_bits - 1)) - cutoff_step_value_t{1}};
    }();

    template <is_fixed cutoff_step_product_t>
        requires(!is_signed_v<cutoff_step_product_t>)
    constexpr auto operator()(cutoff_step_product_t cutoff_step_product) const noexcept -> cutoff_step_t
    {
        static_assert(cutoff_step_t::frac_bits <= cutoff_step_product_t::frac_bits,
            "cutoff-step product alignment must not shift left");

        // clamp
        static constexpr auto ceiling = cutoff_step_product_t::convert(cutoff_ceiling);
        if (cutoff_step_product > ceiling) return cutoff_ceiling;

        // narrow
        static constexpr auto rne_shifter = shifter_t<rounding_modes::shr::nearest_even>{};
        return cutoff_step_t::template convert<rne_shifter>(cutoff_step_product);
    }
};

} // namespace crv::pipeline::filters::one_euro
