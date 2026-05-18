// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "error_norm.hpp"
#include <crv/test/test.hpp>

namespace crv::spline::error_norms {
namespace {

using scalar_t = float_t;

// ====================================================================================================================
// absolute_t
// ====================================================================================================================

namespace absolute_tests {

constexpr auto absolute = absolute_t{};

static_assert(abs(absolute(5.0, 5.0) - 0.0) < 1e-9);
static_assert(abs(absolute(5.0, 3.0) - 2.0) < 1e-9);
static_assert(abs(absolute(3.0, 5.0) - 2.0) < 1e-9);

static_assert(abs(absolute(5.0, -3.0) - 8.0) < 1e-9);
static_assert(abs(absolute(-5.0, 3.0) - 8.0) < 1e-9);
static_assert(abs(absolute(-5.0, -3.0) - 2.0) < 1e-9);

} // namespace absolute_tests

} // namespace
} // namespace crv::spline::error_norms
