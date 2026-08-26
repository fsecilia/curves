// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "validator.hpp"
#include <crv/math/limits.hpp>
#include <crv/pipeline.hpp>
#include <array>

namespace crv::pipeline {
namespace {

struct runtime_config_validator_test_t
{
    using pipeline_t = crv::pipeline_t;
    using config_t = pipeline_t::config_t;
    using gain_t = pipeline_t::gain_t;
    using x_t = gain_t::x_t;
    using y_t = gain_t::y_t;
    using segment_t = gain_t::segment_t;
    using packed_segment_t = segment_t::packed_segment_t;
    using locator_t = gain_t::segment_locator_t;
    using tangent_t = gain_t::extended_tangent_t;
    using coefficient_t = decltype(config_t{}.output_transform)::coefficient_t;

    static constexpr auto make_valid_gain() noexcept -> gain_t
    {
        auto keys = std::array<x_t, locator_t::total_key_count>{};
        for (auto& key : keys) key = x_t{1};

        auto segments = typename gain_t::segments_t{};
        segments[0] = segment_t{packed_segment_t{
            .d = 0,
            .c = 0,
            .b = uint64_t{1} << (y_t::frac_bits + spline::prod_pipeline_config_t::segment_layout.final.shift_width),
            .g0 = y_t{1},
        }};

        return gain_t{
            .segment_locator = locator_t{keys, x_t{1}, 1},
            .segments = segments,
            .extend_final_tangent = tangent_t{
                .slope = {.mantissa = 0, .shift = 0},
                .y0 = y_t{1},
                .x_max_delta = max<x_t>(),
            },
        };
    }

    static constexpr auto make_valid_config() noexcept -> config_t
    {
        return {
            .velocity_scale = pipeline_t::velocity_scale_t{1},
            .half_life = pipeline_t::duration_t{},
            .output_transform = {},
        };
    }
};

using fixture_t = runtime_config_validator_test_t;
using error_t = runtime_config_validation_error_t;

constexpr auto valid_config = fixture_t::make_valid_config();
constexpr auto valid_gain = fixture_t::make_valid_gain();

static_assert(crv::pipeline_t::validate(valid_config, valid_gain) == runtime_config_validation_result_t{});

constexpr auto zero_velocity_scale_config = [] {
    auto config = fixture_t::make_valid_config();
    config.velocity_scale = {};
    return config;
}();
static_assert(crv::pipeline_t::validate(zero_velocity_scale_config, valid_gain).error == error_t::velocity_scale);

constexpr auto max_velocity_scale_config = [] {
    auto config = fixture_t::make_valid_config();
    config.velocity_scale = max<crv::pipeline_t::velocity_scale_t>();
    return config;
}();
static_assert(crv::pipeline_t::validate(max_velocity_scale_config, valid_gain));

constexpr auto max_half_life_config = [] {
    auto config = fixture_t::make_valid_config();
    using filter_t = half_life_ema_t<fixture_t::x_t, crv::pipeline_t::duration_t>;
    config.half_life = filter_t::max_safe_half_life();
    return config;
}();
static_assert(crv::pipeline_t::validate(max_half_life_config, valid_gain));

constexpr auto excessive_half_life_config = [] {
    auto config = max_half_life_config;
    config.half_life.value += 1;
    return config;
}();
static_assert(crv::pipeline_t::validate(excessive_half_life_config, valid_gain).error == error_t::half_life);

constexpr auto invalid_rotation_component_config = [] {
    auto config = fixture_t::make_valid_config();
    config.output_transform.matrix[0][0] = fixture_t::coefficient_t{2};
    return config;
}();
static_assert(crv::pipeline_t::validate(invalid_rotation_component_config, valid_gain).error
    == error_t::output_transform_rotation_component);

constexpr auto invalid_anisotropy_component_config = [] {
    auto config = fixture_t::make_valid_config();
    config.output_transform.matrix[1][0] = fixture_t::coefficient_t{1001};
    return config;
}();
static_assert(crv::pipeline_t::validate(invalid_anisotropy_component_config, valid_gain).error
    == error_t::output_transform_anisotropy_component);

constexpr auto invalid_rotation_norm_config = [] {
    auto config = fixture_t::make_valid_config();
    config.output_transform.matrix[0] = {};
    return config;
}();
static_assert(crv::pipeline_t::validate(invalid_rotation_norm_config, valid_gain).error
    == error_t::output_transform_rotation_norm);

constexpr auto invalid_anisotropy_norm_config = [] {
    auto config = fixture_t::make_valid_config();
    config.output_transform.matrix[1] = {fixture_t::coefficient_t{800}, fixture_t::coefficient_t{800}};
    return config;
}();
static_assert(crv::pipeline_t::validate(invalid_anisotropy_norm_config, valid_gain).error
    == error_t::output_transform_anisotropy_norm);

constexpr auto nonorthogonal_config = [] {
    auto config = fixture_t::make_valid_config();
    config.output_transform.matrix[1] = {fixture_t::coefficient_t{1}, fixture_t::coefficient_t{1}};
    return config;
}();
static_assert(
    crv::pipeline_t::validate(nonorthogonal_config, valid_gain).error == error_t::output_transform_orthogonality);

constexpr auto negative_determinant_config = [] {
    auto config = fixture_t::make_valid_config();
    config.output_transform.matrix[1] = {fixture_t::coefficient_t{}, fixture_t::coefficient_t{-1}};
    return config;
}();
static_assert(
    crv::pipeline_t::validate(negative_determinant_config, valid_gain).error == error_t::output_transform_determinant);

constexpr auto invalid_locator_gain = [] {
    auto gain = fixture_t::make_valid_gain();
    gain.segment_locator = {};
    return gain;
}();
static_assert(crv::pipeline_t::validate(valid_config, invalid_locator_gain).error == error_t::spline_locator);

constexpr auto unsafe_segment_gain = [] {
    auto gain = fixture_t::make_valid_gain();
    constexpr auto max_positive_mantissa = (uint64_t{1} << 56) - 1;
    gain.segments[0] = fixture_t::segment_t{fixture_t::packed_segment_t{
        .d = max_positive_mantissa << spline::prod_pipeline_config_t::segment_layout.intermediate.shift_width,
        .c = 0,
        .b = 0,
        .g0 = {},
    }};
    return gain;
}();
static_assert(crv::pipeline_t::validate(valid_config, unsafe_segment_gain)
    == runtime_config_validation_result_t{.error = error_t::spline_segment, .segment_index = 0});

constexpr auto unsafe_tangent_gain = [] {
    auto gain = fixture_t::make_valid_gain();
    gain.extend_final_tangent.slope.shift = 128;
    return gain;
}();
static_assert(crv::pipeline_t::validate(valid_config, unsafe_tangent_gain).error == error_t::spline_tangent);

constexpr auto mismatched_tangent_gain = [] {
    auto gain = fixture_t::make_valid_gain();
    gain.extend_final_tangent.y0 = fixture_t::y_t{2};
    return gain;
}();
static_assert(crv::pipeline_t::validate(valid_config, mismatched_tangent_gain)
    == runtime_config_validation_result_t{.error = error_t::spline_tangent_anchor, .segment_index = 0});

} // namespace
} // namespace crv::pipeline
