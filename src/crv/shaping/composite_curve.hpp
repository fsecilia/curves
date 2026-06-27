// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>

namespace crv::shaping {

/// composes curve with input and output transforms: out(curve(in(value))) -> value_t
template <typename input_t, typename curve_t, typename output_t> struct composite_curve_t
{
    input_t in;
    curve_t curve;
    output_t out;

    template <typename value_t> [[nodiscard]] constexpr auto operator()(value_t value) const noexcept -> value_t
    {
        return out(curve(in(value)));
    }
};

} // namespace crv::shaping
