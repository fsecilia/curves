// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/kernel/abi.h>

crv_u64_t crv_quantize_timestamp(crv_u64_t timestamp);
void crv_log_timestamp_regression(
    crv_u64_t previous_timestamp, crv_u64_t observed_timestamp, crv_u64_t repaired_timestamp);
