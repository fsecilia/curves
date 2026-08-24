// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "pipeline.h"
#include <crv/kernel/input/abi.h>
#include <crv/pipeline.hpp>
#include <cstddef>
#include <gtest/gtest.h>
#include <vector>

namespace crv {
namespace {

TEST(kernel_pipeline_test, reports_required_tail_storage_size)
{
    EXPECT_EQ(sizeof(pipeline_t) + alignof(pipeline_t) - 1, crv_pipeline_storage_size());
}

TEST(kernel_pipeline_test, constructs_processes_and_destroys_in_c_owned_storage)
{
    auto const storage_size = static_cast<std::size_t>(crv_pipeline_storage_size());
    auto storage = std::vector<std::byte>(storage_size + 1);
    auto* unaligned_storage = static_cast<void*>(storage.data());

    if (reinterpret_cast<uint_t>(unaligned_storage) % alignof(pipeline_t) == 0) unaligned_storage = storage.data() + 1;

    auto* const pipeline = crv_pipeline_construct(unaligned_storage);
    EXPECT_EQ(0U, reinterpret_cast<uint_t>(pipeline) % alignof(pipeline_t));

    auto values = crv_input_value_t{};
    auto const result = crv_pipeline_process(pipeline, &values, 2, 1, 1, 1);

    EXPECT_EQ(CRV_PIPELINE_RESULT_INVALID_REPORT, result.status);
    EXPECT_EQ(2U, result.count);

    crv_pipeline_destroy(pipeline);
}

TEST(kernel_pipeline_test, exposes_split_report_bypass_status_through_c_bridge)
{
    auto const storage_size = static_cast<std::size_t>(crv_pipeline_storage_size());
    auto storage = std::vector<std::byte>(storage_size);
    auto* const pipeline = crv_pipeline_construct(storage.data());

    auto values = std::vector<crv_input_value_t>(4);
    auto const result = crv_pipeline_process(pipeline, values.data(), 2, 4, 3, 1);

    EXPECT_EQ(CRV_PIPELINE_RESULT_SPLIT_REPORT_BYPASSED, result.status);
    EXPECT_EQ(2U, result.count);

    crv_pipeline_destroy(pipeline);
}

} // namespace
} // namespace crv
