// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "scalar_traits.hpp"
#include <crv/test/test.hpp>

namespace crv {
namespace {

// ====================================================================================================================
// Jet Fallbacks
// ====================================================================================================================

using scalar_t = float_t;

static constexpr auto f  = 37.2; // arbitrary
static constexpr auto df = 26.3; // arbitrary

static_assert(f == primal(f));
static_assert(-f == primal(-f));
static_assert(df == primal(df));

static_assert(scalar_t{} == derivative(f));
static_assert(scalar_t{} == derivative(-f));
static_assert(scalar_t{} == derivative(df));

} // namespace
} // namespace crv
