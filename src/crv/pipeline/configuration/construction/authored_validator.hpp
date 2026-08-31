// SPDX-License-Identifier: MIT

/// \file
/// \brief validates authored model values required for runtime construction
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/model/config.hpp>
#include <crv/model/curves/log_normal.hpp>
#include <crv/model/curves/synchronous.hpp>
#include <crv/pipeline.hpp>
#include <crv/tuple.hpp>
#include <cmath>

namespace crv::pipeline::configuration::construction {

enum class authored_validation_error_t : uint8_t
{
    none,
    dpi,
    output_dpi,
    rotation,
    anisotropy,
    filter_half_life,
    filter_half_life_underflow,
    curve_id,
    unsupported_shaping,
    output_scale,
    positioning_mode,
    positioning_height,
    fixed_anchor_negative,
    synchronous_motivity,
    synchronous_gamma,
    synchronous_smooth,
    synchronous_sync_speed,
    log_normal_baseline,
    log_normal_limit,
    log_normal_accel_peak,
    log_normal_max_accel,
    log_normal_parameters,
};

struct authored_validation_result_t
{
    authored_validation_error_t error{};

    constexpr explicit operator bool() const noexcept { return error == authored_validation_error_t::none; }
    constexpr auto operator==(authored_validation_result_t const&) const noexcept -> bool = default;
};

template <typename t_half_life_builder_t> struct authored_validator_t
{
    using half_life_builder_t = t_half_life_builder_t;
    using result_t = authored_validation_result_t;

    [[no_unique_address]] half_life_builder_t build_half_life;

    auto operator()(model::device_t const& device, model::profile_t const& profile) const -> result_t
    {
        if (!satisfies_constraint(device.dpi) || device.dpi.value() == 0)
        {
            return {.error = authored_validation_error_t::dpi};
        }
        if (!satisfies_constraint(profile.output_dpi) || profile.output_dpi.value() == 0
            || static_cast<uint128_t>(profile.output_dpi.value())
                > static_cast<uint128_t>(device.dpi.value()) * pipeline_t::max_output_scale)
        {
            return {.error = authored_validation_error_t::output_dpi};
        }
        if (!satisfies_constraint(device.rotation)) return {.error = authored_validation_error_t::rotation};
        if (!satisfies_constraint(profile.anisotropy)) return {.error = authored_validation_error_t::anisotropy};
        if (!satisfies_constraint(profile.filter_halflife))
        {
            return {.error = authored_validation_error_t::filter_half_life};
        }
        if (profile.filter_halflife.value() > 0
            && build_half_life(profile.filter_halflife.value()) == pipeline_t::duration_t{})
        {
            return {.error = authored_validation_error_t::filter_half_life_underflow};
        }

        auto const curve_index = static_cast<int_t>(profile.curves.active.value());
        if (curve_index < 0 || curve_index >= model::curves::curves_count)
        {
            return {.error = authored_validation_error_t::curve_id};
        }

        auto result = result_t{};
        tuple::visit_at(profile.curves.configs, static_cast<std::size_t>(curve_index),
            [&](auto const& curve_config) { result = validate_curve(curve_config); });
        return result;
    }

private:
    template <typename param_t> static constexpr auto satisfies_constraint(param_t const& param) noexcept -> bool
    {
        auto const value = param.value();
        return param.constraint()(value) == value;
    }

    template <typename curve_config_t> auto validate_curve(curve_config_t const& curve_config) const -> result_t
    {
        auto const common_result = validate_common(curve_config.common);
        if (!common_result) return {.error = common_result.error};

        using specific_t = typename curve_config_t::specific_curve_config_t;
        if constexpr (std::same_as<specific_t, model::curves::synchronous_t::config_t>)
        {
            return validate_synchronous(curve_config.specific);
        }
        else if constexpr (std::same_as<specific_t, model::curves::log_normal_t::config_t>)
        {
            return validate_log_normal(curve_config.specific);
        }
    }

    static auto validate_common(model::common_curve_config_t const& config) -> result_t
    {
        auto const defaults = model::common_curve_config_t{};
        if (config.scale.input != defaults.scale.input || config.offset != defaults.offset
            || config.ceiling != defaults.ceiling)
        {
            return {.error = authored_validation_error_t::unsupported_shaping};
        }

        if (!satisfies_constraint(config.scale.output)) return {.error = authored_validation_error_t::output_scale};
        if (!satisfies_constraint(config.anchor.height))
        {
            return {.error = authored_validation_error_t::positioning_height};
        }

        if (config.anchor.mode.value() != model::anchor_mode_t::offset
            && config.anchor.mode.value() != model::anchor_mode_t::fixed)
        {
            return {.error = authored_validation_error_t::positioning_mode};
        }

        switch (config.anchor.mode.value())
        {
            case model::anchor_mode_t::offset: break;
            case model::anchor_mode_t::fixed:
                if (config.anchor.height.value() < 0)
                {
                    return {.error = authored_validation_error_t::fixed_anchor_negative};
                }
                break;
        }
        return {};
    }

    static auto validate_synchronous(model::curves::synchronous_t::config_t const& config) -> result_t
    {
        if (!satisfies_constraint(config.motivity))
        {
            return {.error = authored_validation_error_t::synchronous_motivity};
        }
        if (!satisfies_constraint(config.gamma)) return {.error = authored_validation_error_t::synchronous_gamma};
        if (!satisfies_constraint(config.smooth)) return {.error = authored_validation_error_t::synchronous_smooth};
        if (!satisfies_constraint(config.sync_speed))
        {
            return {.error = authored_validation_error_t::synchronous_sync_speed};
        }
        return {};
    }

    static auto validate_log_normal(model::curves::log_normal_t::config_t const& config) -> result_t
    {
        if (!satisfies_constraint(config.baseline))
        {
            return {.error = authored_validation_error_t::log_normal_baseline};
        }
        if (!satisfies_constraint(config.limit) || config.limit.value() <= config.baseline.value())
        {
            return {.error = authored_validation_error_t::log_normal_limit};
        }
        if (!satisfies_constraint(config.accel_peak) || config.accel_peak.value() <= 0)
        {
            return {.error = authored_validation_error_t::log_normal_accel_peak};
        }
        if (!satisfies_constraint(config.max_accel) || config.max_accel.value() <= 0)
        {
            return {.error = authored_validation_error_t::log_normal_max_accel};
        }

        auto const params = model::curves::to_params<float_t>(config);
        if (!std::isfinite(params.baseline) || !std::isfinite(params.limit) || !std::isfinite(params.mu)
            || !std::isfinite(params.sigma) || params.sigma <= 0)
        {
            return {.error = authored_validation_error_t::log_normal_parameters};
        }
        return {};
    }
};

} // namespace crv::pipeline::configuration::construction
