// SPDX-License-Identifier: MIT

/// \file
/// \brief validates runtime pipeline configuration before it becomes trusted
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/spline/validator.hpp>
#include <type_traits>

namespace crv::pipeline {

enum class runtime_config_validation_error_t : uint8_t
{
    none,
    velocity_scale,
    output_scale,
    half_life,
    output_transform_rotation_component,
    output_transform_anisotropy_component,
    output_transform_rotation_norm,
    output_transform_anisotropy_norm,
    output_transform_orthogonality,
    output_transform_determinant,
    spline_locator,
    spline_segment,
    spline_tangent,
    spline_tangent_anchor,
};

struct runtime_config_validation_result_t
{
    runtime_config_validation_error_t error{};
    int_t segment_index = -1;

    constexpr explicit operator bool() const noexcept { return error == runtime_config_validation_error_t::none; }
    constexpr auto operator==(runtime_config_validation_result_t const&) const noexcept -> bool = default;
};

/// validates scalar configuration, output transform, and encoded gain spline
template <typename t_config_t, typename t_gain_t, typename t_speed_filter_t> struct runtime_config_validator_t
{
    using config_t = t_config_t;
    using gain_t = t_gain_t;
    using speed_filter_t = t_speed_filter_t;
    using result_t = runtime_config_validation_result_t;

    CRV_ALWAYS_INLINE
    constexpr auto operator()(config_t const& config, gain_t const& gain) const noexcept
        -> runtime_config_validation_result_t
    {
        static_assert(std::is_trivially_copyable_v<config_t>);
        static_assert(std::is_standard_layout_v<config_t>);

        if (config.velocity_scale <= decltype(config.velocity_scale){})
        {
            return {.error = runtime_config_validation_error_t::velocity_scale};
        }
        if (config.half_life > speed_filter_t::max_safe_half_life())
        {
            return {.error = runtime_config_validation_error_t::half_life};
        }

        auto const& transform = config.output_transform;
        if (!transform.output_scale_is_valid()) return {.error = runtime_config_validation_error_t::output_scale};
        if (!transform.rotation_components_are_valid())
        {
            return {.error = runtime_config_validation_error_t::output_transform_rotation_component};
        }
        if (!transform.anisotropy_components_are_valid())
        {
            return {.error = runtime_config_validation_error_t::output_transform_anisotropy_component};
        }
        if (!transform.rotation_norm_is_valid())
        {
            return {.error = runtime_config_validation_error_t::output_transform_rotation_norm};
        }
        if (!transform.anisotropy_norm_is_valid())
        {
            return {.error = runtime_config_validation_error_t::output_transform_anisotropy_norm};
        }
        if (!transform.rows_are_orthogonal())
        {
            return {.error = runtime_config_validation_error_t::output_transform_orthogonality};
        }
        if (!transform.determinant_is_positive())
        {
            return {.error = runtime_config_validation_error_t::output_transform_determinant};
        }

        auto const spline_result = spline::spline_validator_t<gain_t>{}(gain);
        switch (spline_result.error)
        {
            case spline::spline_validation_error_t::none: break;
            case spline::spline_validation_error_t::locator:
                return {.error = runtime_config_validation_error_t::spline_locator};
            case spline::spline_validation_error_t::segment:
                return {
                    .error = runtime_config_validation_error_t::spline_segment,
                    .segment_index = spline_result.segment_index,
                };
            case spline::spline_validation_error_t::tangent:
                return {.error = runtime_config_validation_error_t::spline_tangent};
            case spline::spline_validation_error_t::tangent_anchor:
                return {
                    .error = runtime_config_validation_error_t::spline_tangent_anchor,
                    .segment_index = spline_result.segment_index,
                };
        }

        return {};
    }
};

} // namespace crv::pipeline
