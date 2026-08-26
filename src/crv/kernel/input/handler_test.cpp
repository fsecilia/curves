// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "handler.h"
#include <crv/test/test.hpp>
#include <tuple>

namespace crv {
namespace {

using decision_test_case_t = std::tuple<crv_u32_t, crv_u32_t>;

struct input_handler_decision_test_t : Test
{};

struct input_handler_status_test_t : input_handler_decision_test_t, WithParamInterface<decision_test_case_t>
{};

TEST_P(input_handler_status_test_t, maps_runtime_status_to_passthrough_policy)
{
    constexpr auto input_count = crv_u32_t{3};
    constexpr auto capacity = crv_u32_t{4};
    auto const [status, diagnostic] = GetParam();

    auto const actual
        = crv_input_decide_pipeline_result({.status = status, .count = input_count}, input_count, capacity);
    auto const actual_tuple = std::tuple{actual.count, actual.diagnostic};
    auto const expected = std::tuple{input_count, diagnostic};

    EXPECT_EQ(actual_tuple, expected);
}

INSTANTIATE_TEST_SUITE_P(runtime_statuses, input_handler_status_test_t,
    Values(decision_test_case_t{CRV_PIPELINE_RESULT_APPLIED, CRV_INPUT_PIPELINE_DIAGNOSTIC_NONE},
        decision_test_case_t{CRV_PIPELINE_RESULT_WARMUP, CRV_INPUT_PIPELINE_DIAGNOSTIC_NONE},
        decision_test_case_t{CRV_PIPELINE_RESULT_INACTIVE, CRV_INPUT_PIPELINE_DIAGNOSTIC_NONE},
        decision_test_case_t{CRV_PIPELINE_RESULT_SPLIT_REPORT_BYPASSED, CRV_INPUT_PIPELINE_DIAGNOSTIC_SPLIT_REPORT},
        decision_test_case_t{CRV_PIPELINE_RESULT_INVALID_REPORT, CRV_INPUT_PIPELINE_DIAGNOSTIC_INVALID_REPORT},
        decision_test_case_t{CRV_PIPELINE_RESULT_INVALID_TIMESTAMP, CRV_INPUT_PIPELINE_DIAGNOSTIC_INVALID_TIMESTAMP},
        decision_test_case_t{
            CRV_PIPELINE_RESULT_VELOCITY_OUT_OF_RANGE, CRV_INPUT_PIPELINE_DIAGNOSTIC_VELOCITY_OUT_OF_RANGE},
        decision_test_case_t{CRV_PIPELINE_RESULT_TRANSFORM_INPUT_OUT_OF_RANGE,
            CRV_INPUT_PIPELINE_DIAGNOSTIC_TRANSFORM_INPUT_OUT_OF_RANGE},
        decision_test_case_t{
            CRV_PIPELINE_RESULT_OUTPUT_OUT_OF_RANGE, CRV_INPUT_PIPELINE_DIAGNOSTIC_OUTPUT_OUT_OF_RANGE},
        decision_test_case_t{CRV_PIPELINE_RESULT_APPEND_FAILED, CRV_INPUT_PIPELINE_DIAGNOSTIC_APPEND_FAILED}));

TEST_F(input_handler_decision_test_t, malformed_count_is_dropped)
{
    auto const actual
        = crv_input_decide_pipeline_result({.status = CRV_PIPELINE_RESULT_INVALID_REPORT, .count = 5}, 5, 4);
    auto const actual_tuple = std::tuple{actual.count, actual.diagnostic};
    auto const expected = std::tuple{crv_u32_t{0}, crv_u32_t{CRV_INPUT_PIPELINE_DIAGNOSTIC_INVALID_COUNT}};

    EXPECT_EQ(actual_tuple, expected);
}

TEST_F(input_handler_decision_test_t, impossible_pipeline_count_is_dropped)
{
    auto const actual = crv_input_decide_pipeline_result({.status = CRV_PIPELINE_RESULT_APPLIED, .count = 5}, 3, 4);
    auto const actual_tuple = std::tuple{actual.count, actual.diagnostic};
    auto const expected = std::tuple{crv_u32_t{0}, crv_u32_t{CRV_INPUT_PIPELINE_DIAGNOSTIC_IMPOSSIBLE_COUNT}};

    EXPECT_EQ(actual_tuple, expected);
}

TEST_F(input_handler_decision_test_t, unknown_status_preserves_safe_input_count)
{
    auto const actual = crv_input_decide_pipeline_result({.status = 99, .count = 1}, 3, 4);
    auto const actual_tuple = std::tuple{actual.count, actual.diagnostic};
    auto const expected = std::tuple{crv_u32_t{3}, crv_u32_t{CRV_INPUT_PIPELINE_DIAGNOSTIC_UNKNOWN_STATUS}};

    EXPECT_EQ(actual_tuple, expected);
}

} // namespace
} // namespace crv
