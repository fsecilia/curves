// SPDX-License-Identifier: MIT

/// \file
/// \brief authored scalar configuration to production runtime configuration
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/rounding_mode.hpp>
#include <crv/model/config.hpp>
#include <crv/pipeline.hpp>
#include <cassert>

namespace crv::pipeline::configuration::construction {

struct runtime_units_t
{
    static constexpr auto reference_dpi = uint64_t{1000};
    static constexpr auto nanoseconds_per_millisecond = uint64_t{1'000'000};
};

struct velocity_scale_builder_t
{
    using out_t = pipeline_t::velocity_scale_t;
    using dpi_t = fixed_t<uint64_t, 0>;

    constexpr auto operator()(int_t dpi) const noexcept -> out_t
    {
        assert(dpi > 0 && "velocity_scale_builder_t: dpi must be positive");

        static constexpr auto numerator
            = out_t{runtime_units_t::reference_dpi * runtime_units_t::nanoseconds_per_millisecond};
        return divide<out_t>(numerator, dpi_t{static_cast<uint64_t>(dpi)}, rounding_modes::div::nearest_even);
    }
};

struct half_life_builder_t
{
    using out_t = pipeline_t::duration_t;

    constexpr auto operator()(float_t milliseconds) const noexcept -> out_t
    {
        assert(milliseconds >= 0 && "half_life_builder_t: half-life must be nonnegative");
        return to_fixed<out_t>(milliseconds * static_cast<float_t>(runtime_units_t::nanoseconds_per_millisecond));
    }
};

template <typename t_velocity_scale_builder_t, typename t_half_life_builder_t, typename t_output_transform_builder_t>
struct config_builder_t
{
    using velocity_scale_builder_t = t_velocity_scale_builder_t;
    using half_life_builder_t = t_half_life_builder_t;
    using output_transform_builder_t = t_output_transform_builder_t;

    [[no_unique_address]] velocity_scale_builder_t build_velocity_scale;
    [[no_unique_address]] half_life_builder_t build_half_life;
    [[no_unique_address]] output_transform_builder_t build_output_transform;

    auto operator()(model::device_t const& device, model::profile_t const& profile) const noexcept
        -> pipeline_t::config_t
    {
        return {
            .velocity_scale = build_velocity_scale(device.dpi.value()),
            .half_life = build_half_life(profile.filter_halflife.value()),
            .output_transform = build_output_transform(
                device.rotation.value(), profile.anisotropy.value(), device.dpi.value(), profile.output_dpi.value()),
        };
    }
};

} // namespace crv::pipeline::configuration::construction
