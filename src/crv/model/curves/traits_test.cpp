// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "traits.hpp"
#include <crv/math/jet/jet.hpp>
#include <crv/test/test.hpp>

namespace crv {
namespace {

struct arbitrary_t
{};

//
// is_curve_scalar
//

static_assert(!is_curve_scalar<arbitrary_t>);
static_assert(!is_curve_scalar<int_t>);
static_assert(!is_curve_scalar<jet_t<float32_t>>);
static_assert(!is_curve_scalar<jet_t<float64_t>>);

static_assert(is_curve_scalar<float32_t>);
static_assert(is_curve_scalar<float64_t>);
static_assert(is_curve_scalar<std::complex<float32_t>>);
static_assert(is_curve_scalar<std::complex<float64_t>>);

} // namespace
} // namespace crv
