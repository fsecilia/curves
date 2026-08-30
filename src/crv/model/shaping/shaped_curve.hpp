// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>

namespace crv::shaping {

/// shapes curve, composing it with input and output transforms: out(curve(in(value))) -> value_t
template <typename input_t, typename curve_t, typename output_t> struct shaped_curve_t
{
    input_t in;
    curve_t curve;
    output_t out;

    template <typename value_t> [[nodiscard]] constexpr auto operator()(value_t value) const noexcept -> value_t
    {
        return out(curve(in(value)));
    }
};

/// temporary standin builder that leaves the curve unchanged
struct shaped_curve_builder_t
{
    template <typename curve_t> using result_t = curve_t;

    template <typename curve_t> constexpr auto operator()(curve_t curve) const noexcept -> result_t<curve_t>
    {
        return curve;
    }
};

} // namespace crv::shaping
