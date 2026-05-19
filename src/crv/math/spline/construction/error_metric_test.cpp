// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "error_metric.hpp"
#include <crv/math/jet/jet.hpp>
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

using scalar_t = float_t;

constexpr auto sut = error_metric_t{};

static_assert(abs(sut(5.0, 5.0) - 0.0) < 1e-9);
static_assert(abs(sut(5.0, 3.0) - 2.0) < 1e-9);
static_assert(abs(sut(3.0, 5.0) - 2.0) < 1e-9);

static_assert(abs(sut(5.0, -3.0) - 8.0) < 1e-9);
static_assert(abs(sut(-5.0, 3.0) - 8.0) < 1e-9);
static_assert(abs(sut(-5.0, -3.0) - 2.0) < 1e-9);

} // namespace
} // namespace crv::spline
