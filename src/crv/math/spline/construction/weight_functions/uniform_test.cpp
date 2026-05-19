// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "uniform.hpp"
#include <crv/test/test.hpp>

namespace crv::spline::weight_functions {
namespace {

using scalar_t = float_t;

constexpr auto sut = uniform_t<scalar_t>{};

static_assert(sut(0.0) == 1.0);
static_assert(sut(0.5) == 1.0);
static_assert(sut(1.0) == 1.0);
static_assert(sut(10.0) == 1.0);

} // namespace
} // namespace crv::spline::weight_functions
