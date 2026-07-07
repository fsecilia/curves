// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <optional>

namespace crv::shaping::config {

struct affine_t
{
    float_t scale{1.0};
    float_t shift{0.0};
};

struct offset_t
{
    float_t start{0.0};
    float_t width{0.0};
};

struct limit_t
{
    float_t limit{0.0};
    float_t width{0.0};
};

struct common_curve_t
{
    affine_t input;
    affine_t output;
    offset_t offset;
    limit_t limit;
};

} // namespace crv::shaping::config
