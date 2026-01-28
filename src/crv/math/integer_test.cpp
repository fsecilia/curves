// SPDX-License-Identifier: MIT
/**
  \file
  \copyright Copyright (C) 2026 Frank Secilia
*/

#include <crv/math/integer.hpp>
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

// --------------------------------------------------------------------------------------------------------------------
// sized_integer_t
// --------------------------------------------------------------------------------------------------------------------

namespace sized_integer_tests {

/*
    gcc's implementation of std::is_signed is conditional on std::is_arithmetic. int128 is not arithmetic, so checking
    it against is_signed fails. This implementation does the same check as is_signed, without being obstructed by
    is_arithmetic.
*/
template <typename value_t> constexpr auto is_signed_v = value_t(-1) < value_t(0);

// tests is_integral, is_signed, and size for the given sized_integer_t
template <int_t expected_size, bool expected_is_signed> constexpr auto test_size() noexcept -> void
{
    using actual_t = sized_integer_t<expected_size, expected_is_signed>;

    static_assert(integral<actual_t>, "sized_integer_t: result was not integral");
    static_assert(expected_size == sizeof(actual_t), "sized_integer_t: size did not match");
    static_assert(expected_is_signed == is_signed_v<actual_t>, "sized_integer_t: is_signed did not match");
};

// runs signed and unsigned test_size() for given sizes
template <int_t... sizes> constexpr auto test_sizes() noexcept -> void
{
    (..., (test_size<sizes, true>(), test_size<sizes, false>()));
}

// runs test_sizes() for all supported integer sizes
constexpr auto test_all_sizes() noexcept -> void
{
    test_sizes<1, 2, 4, 8, 16>();
}

} // namespace sized_integer_tests

} // namespace
} // namespace crv
