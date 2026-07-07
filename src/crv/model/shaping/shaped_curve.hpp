// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <utility>

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

/// temporary standin builder that just produces a valid curve with identity transforms
struct shaped_curve_builder_t
{
    struct identity_transform_t
    {
        template <typename value_t> constexpr auto operator()(value_t value) const noexcept -> value_t { return value; }
    };

    template <typename curve_t> using result_t = shaped_curve_t<identity_transform_t, curve_t, identity_transform_t>;

    template <typename curve_t> constexpr auto operator()(curve_t curve) const noexcept -> result_t<curve_t>
    {
        return {.in = identity_transform_t{}, .curve = std::move(curve), .out = identity_transform_t{}};
    }
};

} // namespace crv::shaping
