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

    static constexpr auto segment_layout = t_segment_layout;
};

constexpr auto prod_pipeline_config = pipeline_config_t<fixed_t<int64_t, 42>, fixed_t<int64_t, 45>,
    segment_layout_t<field_layout_t<uint64_t, int_t>>{
        .intermediate = {.shift_width = 6, .is_signed = false},
        .final = {.shift_width = 7, .is_signed = true},
    }>{};

using prod_pipeline_config_t = decltype(prod_pipeline_config);

} // namespace crv::spline
