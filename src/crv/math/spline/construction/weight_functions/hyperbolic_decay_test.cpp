// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "hyperbolic_decay.hpp"
#include <crv/test/test.hpp>

namespace crv::spline::weight_functions {
namespace {

using scalar_t = float_t;

using sut_t = hyperbolic_decay_t<scalar_t>;

static_assert(sut_t{0}(1.0) == 1.0 / 1.0);
static_assert(sut_t{0}(2.0) == 1.0 / 2.0);
static_assert(sut_t{0}(7.0) == 1.0 / 7.0);
static_assert(sut_t{0}(11.0) == 1.0 / 11.0);

static_assert(sut_t{3.1}(1.0) == 1.0 / (1.0 + 3.1));
static_assert(sut_t{3.1}(2.0) == 1.0 / (2.0 + 3.1));
static_assert(sut_t{3.1}(7.0) == 1.0 / (7.0 + 3.1));
static_assert(sut_t{3.1}(11.0) == 1.0 / (11.0 + 3.1));

} // namespace
} // namespace crv::spline::weight_functions
