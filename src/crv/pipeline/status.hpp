// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>

namespace crv::pipeline {

enum class pipeline_result_t : uint32_t
{
    applied = 0,
    warmup = 1,
    invalid_report = 2,
    invalid_timestamp = 3,
    velocity_out_of_range = 4,
    transform_input_out_of_range = 5,
    output_out_of_range = 6,
    append_failed = 7,
    split_report_bypassed = 8,
};

} // namespace crv::pipeline
