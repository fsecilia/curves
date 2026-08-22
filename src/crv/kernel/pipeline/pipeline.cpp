// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "pipeline.h"
#include <crv/pipeline.hpp>
#include <cassert>
#include <new>

namespace {

constexpr auto pipeline_alignment = alignof(crv::pipeline_t);
constexpr auto pipeline_storage_size = sizeof(crv::pipeline_t) + pipeline_alignment - 1;

static_assert((pipeline_alignment & (pipeline_alignment - 1)) == 0);
static_assert(pipeline_storage_size <= ~crv_u32_t{});
static_assert(sizeof(crv::pipeline_t::timestamp_t) == sizeof(crv_u64_t));

inline auto cpp_pipeline(crv_pipeline* pipeline) noexcept -> crv::pipeline_t*
{
    assert(nullptr != pipeline);
    return reinterpret_cast<crv::pipeline_t*>(pipeline);
}

inline auto align_pipeline_storage(void* storage) noexcept -> void*
{
    assert(nullptr != storage);

    auto const address = reinterpret_cast<crv::uint_t>(storage);
    auto const aligned_address = (address + pipeline_alignment - 1) & ~(pipeline_alignment - 1);
    return reinterpret_cast<void*>(aligned_address);
}

} // namespace

extern "C" auto crv_pipeline_storage_size(void) -> crv_u32_t
{
    return pipeline_storage_size;
}

extern "C" auto crv_pipeline_construct(void* storage) -> crv_pipeline*
{
    auto* const pipeline = ::new (align_pipeline_storage(storage)) crv::pipeline_t{};
    return reinterpret_cast<crv_pipeline*>(pipeline);
}

extern "C" auto crv_pipeline_destroy(crv_pipeline* pipeline) -> void
{
    cpp_pipeline(pipeline)->~pipeline_t();
}

extern "C" auto crv_pipeline_process(
    crv_pipeline* pipeline, void* values, crv_u32_t count, crv_u32_t capacity, crv_u64_t timestamp)
    -> crv_pipeline_result_t
{
    auto const result = (*cpp_pipeline(pipeline))(values, count, capacity, timestamp);

    return {
        .status = static_cast<crv_u32_t>(result.status),
        .count = static_cast<crv_u32_t>(result.count),
    };
}
