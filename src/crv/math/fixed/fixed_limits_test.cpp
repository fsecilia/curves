// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "fixed_limits.hpp"
#include <crv/math/fixed/io.hpp>
#include <crv/math/io.hpp>
#include <crv/test/test.hpp>
#include <limits>

namespace crv {
namespace {

using value_t = int32_t;
using sut_t = std::numeric_limits<fixed_t<int32_t, 15>>;
using base_t = std::numeric_limits<value_t>;

// traits
static_assert(sut_t::is_specialized);
static_assert(!sut_t::is_integer);
static_assert(sut_t::is_exact);
static_assert(sut_t::is_signed == base_t::is_signed);
static_assert(!sut_t::has_infinity);
static_assert(!sut_t::has_quiet_NaN);

// values
static_assert(sut_t::max().value == base_t::max());
static_assert(sut_t::lowest().value == base_t::lowest());
static_assert(sut_t::min().value == 1); // min() is the smallest positive value, not the most negative

// epsilon is the constant step size
static_assert(sut_t::epsilon().value == 1);

} // namespace
} // namespace crv
