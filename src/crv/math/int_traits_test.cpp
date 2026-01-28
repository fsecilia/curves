// SPDX-License-Identifier: MIT
/**
  \file
  \copyright Copyright (C) 2026 Frank Secilia
*/

#include <crv/math/int_traits.hpp>
#include <crv/test/test.hpp>

namespace crv {
namespace {

// --------------------------------------------------------------------------------------------------------------------
// is_integral
// --------------------------------------------------------------------------------------------------------------------

namespace is_integral_tests {

static_assert(integral<int8_t>);
static_assert(integral<int64_t>);
static_assert(integral<int128_t>);

static_assert(integral<uint8_t>);
static_assert(integral<uint64_t>);
static_assert(integral<uint128_t>);

static_assert(!integral<float>);
static_assert(!integral<double>);
static_assert(!integral<long double>);

struct nonintegral_t
{};

static_assert(!integral<nonintegral_t>);

} // namespace is_integral_tests

// --------------------------------------------------------------------------------------------------------------------
// is_arithmetic
// --------------------------------------------------------------------------------------------------------------------

namespace is_arithmetic_tests {

static_assert(arithmetic<int8_t>);
static_assert(arithmetic<int64_t>);
static_assert(arithmetic<int128_t>);

static_assert(arithmetic<uint8_t>);
static_assert(arithmetic<uint64_t>);
static_assert(arithmetic<uint128_t>);

static_assert(arithmetic<float>);
static_assert(arithmetic<double>);
static_assert(arithmetic<long double>);

struct nonarithmetic_t
{};

static_assert(!arithmetic<nonarithmetic_t>);

} // namespace is_arithmetic_tests

} // namespace
} // namespace crv
