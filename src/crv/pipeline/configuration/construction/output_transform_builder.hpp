// SPDX-License-Identifier: MIT

/// \file
/// \brief userspace construction of the production output transform
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/rounding_mode.hpp>
#include <cassert>
#include <cmath>
#include <numbers>

namespace crv::pipeline::configuration::construction {

template <typename t_transform_t> struct output_transform_builder_t
{
    using transform_t = t_transform_t;

    auto operator()(float_t rotation_degrees, float_t anisotropy, int_t input_dpi, int_t output_dpi) const noexcept
        -> transform_t
    {
        assert(input_dpi > 0 && "output_transform_builder_t: input DPI must be positive");
        assert(output_dpi > 0 && "output_transform_builder_t: output DPI must be positive");
        using std::cos;
        using std::sin;

        auto const radians = rotation_degrees * std::numbers::pi_v<float_t> / float_t{180};
        auto const cosine = cos(radians);
        auto const sine = sin(radians);
        using coefficient_t = transform_t::coefficient_t;
        using dpi_t = fixed_t<uint64_t, 0>;
        using scale_t = transform_t::scale_t;
        auto const output_scale = divide<scale_t>(dpi_t{static_cast<uint64_t>(output_dpi)},
            dpi_t{static_cast<uint64_t>(input_dpi)}, rounding_modes::div::nearest_even);

        return transform_t{
            .matrix = {{
                {to_fixed<coefficient_t>(cosine), to_fixed<coefficient_t>(-sine)},
                {to_fixed<coefficient_t>(anisotropy * sine), to_fixed<coefficient_t>(anisotropy * cosine)},
            }},
            .output_scale = output_scale,
        };
    }
};

} // namespace crv::pipeline::configuration::construction
