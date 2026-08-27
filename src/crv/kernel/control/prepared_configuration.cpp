// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "prepared_configuration.h"
#include "prepared_configuration.hpp"
#include <crv/pipeline.hpp>
#include <crv/pipeline/configuration/candidate.hpp>
#include <crv/pipeline/configuration/committer.hpp>
#include <crv/pipeline/configuration/transaction.hpp>
#include <cassert>
#include <cstddef>
#include <new>
#include <type_traits>

namespace crv::kernel::control {
namespace {

using candidate_t = pipeline::configuration::candidate_t;
using transaction_t
    = pipeline::configuration::transaction_t<pipeline_t::validator_t, pipeline::configuration::committer_t>;
using prepared_t = prepared_configuration_t<candidate_t, transaction_t>;
using config_t = pipeline_t::config_t;
using gain_t = pipeline_t::gain_t;
using output_transform_t = decltype(config_t::output_transform);
using validation_error_t = pipeline::runtime_config_validation_error_t;

static_assert(
    apply_mode_decoder_t{}(CRV_CONTROL_APPLY_MODE_BYPASSED) == pipeline::configuration::apply_mode_t::bypassed);
static_assert(apply_mode_decoder_t{}(CRV_CONTROL_APPLY_MODE_ACTIVE) == pipeline::configuration::apply_mode_t::active);
static_assert(!apply_mode_decoder_t{}(2));

constexpr auto prepared_alignment = alignof(prepared_t);
constexpr auto prepared_storage_size = sizeof(prepared_t) + prepared_alignment - 1;

static_assert((prepared_alignment & (prepared_alignment - 1)) == 0);
static_assert(prepared_alignment >= alignof(candidate_t));
static_assert(prepared_storage_size <= ~crv_u32_t{});

static_assert(sizeof(config_t) == sizeof(crv_control_runtime_config_v1_t));
static_assert(alignof(config_t) == alignof(crv_control_runtime_config_v1_t));
static_assert(offsetof(config_t, velocity_scale) == offsetof(crv_control_runtime_config_v1_t, velocity_scale));
static_assert(offsetof(config_t, half_life) == offsetof(crv_control_runtime_config_v1_t, half_life));
static_assert(offsetof(config_t, output_transform) == offsetof(crv_control_runtime_config_v1_t, output_transform));
static_assert(offsetof(config_t, output_transform) + offsetof(output_transform_t, output_scale)
    == offsetof(crv_control_runtime_config_v1_t, output_scale));
static_assert(sizeof(decltype(config_t::velocity_scale)) == sizeof(crv_u64_t));
static_assert(sizeof(decltype(config_t::half_life)) == sizeof(crv_u64_t));
static_assert(sizeof(decltype(output_transform_t::matrix)) == sizeof(crv_s64_t) * 4);
static_assert(sizeof(decltype(output_transform_t::output_scale)) == sizeof(crv_u64_t));
static_assert(std::is_trivially_copyable_v<config_t>);
static_assert(std::is_standard_layout_v<config_t>);

static_assert(offsetof(gain_t, segment_locator) == sizeof(crv_u64_t) * CRV_CONTROL_GAIN_V1_LOCATOR_WORD_OFFSET);
static_assert(offsetof(gain_t, segments) == sizeof(crv_u64_t) * CRV_CONTROL_GAIN_V1_SEGMENTS_WORD_OFFSET);
static_assert(offsetof(gain_t, extend_final_tangent) == sizeof(crv_u64_t) * CRV_CONTROL_GAIN_V1_TANGENT_WORD_OFFSET);
static_assert(sizeof(gain_t::segment_locator_t) == sizeof(crv_u64_t) * CRV_CONTROL_GAIN_V1_SEGMENTS_WORD_OFFSET);
static_assert(sizeof(gain_t::segments_t)
    == sizeof(crv_u64_t) * (CRV_CONTROL_GAIN_V1_TANGENT_WORD_OFFSET - CRV_CONTROL_GAIN_V1_SEGMENTS_WORD_OFFSET));
static_assert(sizeof(gain_t::extended_tangent_t) == sizeof(crv_u64_t) * CRV_CONTROL_GAIN_V1_TANGENT_WORD_COUNT);
static_assert(
    offsetof(gain_t, extend_final_tangent) + sizeof(gain_t::extended_tangent_t) == sizeof(crv_control_gain_v1_t));
static_assert(sizeof(gain_t) - sizeof(crv_control_gain_v1_t) == 32);
static_assert(std::is_trivially_copyable_v<gain_t>);
static_assert(std::is_standard_layout_v<gain_t>);

static_assert(sizeof(crv_control_configuration_v1_t) == sizeof(config_t) + sizeof(crv_control_gain_v1_t));
static_assert(offsetof(crv_control_configuration_v1_t, config) == 0);
static_assert(offsetof(crv_control_configuration_v1_t, gain) == sizeof(config_t));

inline auto cpp_configuration(crv_control_prepared_configuration_t* configuration) noexcept -> prepared_t*
{
    assert(nullptr != configuration);
    return reinterpret_cast<prepared_t*>(configuration);
}

inline auto cpp_configuration(crv_control_prepared_configuration_t const* configuration) noexcept -> prepared_t const*
{
    assert(nullptr != configuration);
    return reinterpret_cast<prepared_t const*>(configuration);
}

inline auto cpp_pipeline(crv_pipeline_t* pipeline) noexcept -> pipeline_t*
{
    assert(nullptr != pipeline);
    return reinterpret_cast<pipeline_t*>(pipeline);
}

inline auto align_storage(void* storage) noexcept -> void*
{
    assert(nullptr != storage);

    auto const address = reinterpret_cast<uint_t>(storage);
    auto const aligned_address = (address + prepared_alignment - 1) & ~(prepared_alignment - 1);
    return reinterpret_cast<void*>(aligned_address);
}

inline auto destroy_configuration(crv_control_prepared_configuration_t* configuration) noexcept -> void
{
    cpp_configuration(configuration)->~prepared_configuration_t();
}

constexpr auto validation_error_name(validation_error_t error) noexcept -> char const*
{
    switch (error)
    {
        case validation_error_t::none: return "none";
        case validation_error_t::velocity_scale: return "velocity scale";
        case validation_error_t::output_scale: return "output scale";
        case validation_error_t::half_life: return "half life";
        case validation_error_t::output_transform_rotation_component: return "output transform rotation component";
        case validation_error_t::output_transform_anisotropy_component: return "output transform anisotropy component";
        case validation_error_t::output_transform_rotation_norm: return "output transform rotation norm";
        case validation_error_t::output_transform_anisotropy_norm: return "output transform anisotropy norm";
        case validation_error_t::output_transform_orthogonality: return "output transform orthogonality";
        case validation_error_t::output_transform_determinant: return "output transform determinant";
        case validation_error_t::spline_locator: return "spline locator";
        case validation_error_t::spline_segment: return "spline segment";
        case validation_error_t::spline_tangent: return "spline tangent";
        case validation_error_t::spline_tangent_anchor: return "spline tangent anchor";
    }

    return "unknown";
}

} // namespace
} // namespace crv::kernel::control

extern "C" auto crv_control_prepared_configuration_storage_size(void) -> crv_u32_t
{
    return crv::kernel::control::prepared_storage_size;
}

extern "C" auto crv_control_prepared_configuration_construct(void* storage, crv_u32_t mode)
    -> crv_control_prepared_configuration_t*
{
    using namespace crv::kernel::control;

    auto const decoded_mode = apply_mode_decoder_t{}(mode);
    if (!decoded_mode) return nullptr;

    auto* const configuration = ::new (align_storage(storage)) prepared_t{*decoded_mode};
    return reinterpret_cast<crv_control_prepared_configuration_t*>(configuration);
}

extern "C" auto crv_control_prepared_configuration_destroy(crv_control_prepared_configuration_t* configuration) -> void
{
    crv::kernel::control::destroy_configuration(configuration);
}

extern "C" auto crv_control_prepared_configuration_config(crv_control_prepared_configuration_t* configuration) -> void*
{
    auto bytes = crv::kernel::control::cpp_configuration(configuration)->config_bytes();
    return bytes.data();
}

extern "C" auto crv_control_prepared_configuration_config_size(void) -> crv_u32_t
{
    return sizeof(crv::kernel::control::config_t);
}

extern "C" auto crv_control_prepared_configuration_gain(crv_control_prepared_configuration_t* configuration) -> void*
{
    auto bytes = crv::kernel::control::cpp_configuration(configuration)->gain_bytes();
    return bytes.data();
}

extern "C" auto crv_control_prepared_configuration_gain_size(void) -> crv_u32_t
{
    return sizeof(crv_control_gain_v1_t);
}

extern "C" auto crv_control_prepared_configuration_validate(crv_control_prepared_configuration_t* configuration)
    -> crv_control_validation_result_t
{
    auto const result = crv::kernel::control::cpp_configuration(configuration)->validate();
    return {
        .error = static_cast<crv_u32_t>(result.error),
        .segment_index = static_cast<crv_s64_t>(result.segment_index),
    };
}

extern "C" auto crv_control_validation_error_name(crv_u32_t error) -> char const*
{
    using namespace crv::kernel::control;

    if (error > static_cast<crv_u32_t>(validation_error_t::spline_tangent_anchor)) return "unknown";
    return validation_error_name(static_cast<validation_error_t>(error));
}

extern "C" auto crv_control_prepared_configuration_commit(
    crv_control_prepared_configuration_t const* configuration, crv_pipeline_t* pipeline) -> void
{
    crv::kernel::control::cpp_configuration(configuration)->commit(*crv::kernel::control::cpp_pipeline(pipeline));
}
