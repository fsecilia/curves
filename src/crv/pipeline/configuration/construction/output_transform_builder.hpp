// SPDX-License-Identifier: MIT

/// \file
/// \brief userspace construction of the production output transform
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <cmath>
#include <numbers>

namespace crv::pipeline {

template <typename t_transform_t> struct output_transform_builder_t
{
    using transform_t = t_transform_t;

    auto operator()(float_t rotation_degrees, float_t anisotropy) const noexcept -> transform_t
    {
        using std::cos;
        using std::sin;

        auto const radians = rotation_degrees * std::numbers::pi_v<float_t> / float_t{180};
        auto const cosine = cos(radians);
        auto const sine = sin(radians);
        using coefficient_t = transform_t::coefficient_t;

        return transform_t{.matrix = {{
                               {to_fixed<coefficient_t>(cosine), to_fixed<coefficient_t>(-sine)},
                               {to_fixed<coefficient_t>(anisotropy * sine),
                                   to_fixed<coefficient_t>(anisotropy * cosine)},
                           }}};
    }
};

} // namespace crv::pipeline
