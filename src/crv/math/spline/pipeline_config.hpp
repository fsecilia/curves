// SPDX-License-Identifier: MIT

/// \file
/// \brief shared production pipline configuration
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/spline/segment.hpp>

namespace crv::spline {

template <typename t_x_t, typename t_y_t, segment_layout_t t_segment_layout> struct pipeline_config_t
{
    using x_t = t_x_t;
    using y_t = t_y_t;

    segment_layout_t segment_layout = t_segment_layout;
};

// x_t = fixed_t<int64_t, 42>
//
// A 32khz mouse fully saturating input at max rate produces a velocity of sqrt(2*(2^15 - 1)^2)*32 ~= 20.5 bits, so
// we need 21 integer bits, which gives Q21.42.

// y_t = fixed_t<int64_t, 45>
//
// For a domain of [0, 2^8) and soft limiter on the integrand at y=1000, the largest integral possible is a pinned
// straight line, integrating to 256000. The integer limit of Q18.45 is 262144, giving about 0.6% headroom.

constexpr auto prod_pipeline_config = pipeline_config_t<fixed_t<int64_t, 42>, fixed_t<int64_t, 45>,
    segment_layout_t{
        .intermediate = {.shift_width = 6, .is_signed = false},
        .final = {.shift_width = 7, .is_signed = true},
    }>{};

using prod_pipeline_config_t = decltype(prod_pipeline_config);

} // namespace crv::spline
