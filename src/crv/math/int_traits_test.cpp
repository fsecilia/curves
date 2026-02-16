// SPDX-License-Identifier: MIT
/**
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include "int_traits.hpp"
#include <crv/test/test.hpp>

namespace crv {
namespace {

// --------------------------------------------------------------------------------------------------------------------
// integral
// --------------------------------------------------------------------------------------------------------------------

namespace is_integral_tests {

static_assert(integral<int8_t>);
static_assert(integral<int16_t const>);
static_assert(integral<int32_t volatile>);
static_assert(integral<int64_t>);
static_assert(integral<int128_t>);

static_assert(integral<uint8_t>);
static_assert(integral<uint16_t const>);
static_assert(integral<uint32_t volatile>);
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
// arithmetic
// --------------------------------------------------------------------------------------------------------------------

namespace is_arithmetic_tests {

static_assert(arithmetic<int8_t>);
static_assert(arithmetic<int16_t const>);
static_assert(arithmetic<int32_t volatile>);
static_assert(arithmetic<int64_t>);
static_assert(arithmetic<int128_t>);

static_assert(arithmetic<uint8_t>);
static_assert(arithmetic<uint16_t const>);
static_assert(arithmetic<uint32_t volatile>);
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
// signed
// --------------------------------------------------------------------------------------------------------------------

namespace is_signed_tests {

static_assert(signed_integral<int8_t>);
static_assert(signed_integral<int16_t const>);
static_assert(signed_integral<int32_t volatile>);
static_assert(signed_integral<int64_t>);
static_assert(signed_integral<int128_t>);

static_assert(!signed_integral<uint8_t>);
static_assert(!signed_integral<uint16_t const>);
static_assert(!signed_integral<uint32_t volatile>);
static_assert(!signed_integral<uint64_t>);
static_assert(!signed_integral<uint128_t>);

static_assert(!signed_integral<float>);
static_assert(!signed_integral<double>);
static_assert(!signed_integral<long double>);

static_assert(!unsigned_integral<int8_t>);
static_assert(!unsigned_integral<int16_t const>);
static_assert(!unsigned_integral<int32_t volatile>);
static_assert(!unsigned_integral<int64_t>);
static_assert(!unsigned_integral<int128_t>);

static_assert(unsigned_integral<uint8_t>);
static_assert(unsigned_integral<uint16_t const>);
static_assert(unsigned_integral<uint32_t volatile>);
static_assert(unsigned_integral<uint64_t>);
static_assert(unsigned_integral<uint128_t>);

static_assert(!unsigned_integral<float>);
static_assert(!unsigned_integral<double>);
static_assert(!unsigned_integral<long double>);

struct nonsigned_t
{};

static_assert(!signed_integral<nonsigned_t>);
static_assert(!unsigned_integral<nonsigned_t>);

} // namespace is_signed_tests

// --------------------------------------------------------------------------------------------------------------------
// make_unsigned
// --------------------------------------------------------------------------------------------------------------------

namespace make_unsigned_tests {

// basic types match std::make_unsigned
static_assert(std::same_as<make_unsigned_t<int>, std::make_unsigned_t<int>>);
static_assert(std::same_as<make_unsigned_t<signed>, std::make_unsigned_t<signed>>);
static_assert(std::same_as<make_unsigned_t<unsigned>, std::make_unsigned_t<unsigned>>);
static_assert(std::same_as<make_unsigned_t<unsigned int>, std::make_unsigned_t<unsigned int>>);
static_assert(std::same_as<make_unsigned_t<int8_t>, uint8_t>);
static_assert(std::same_as<make_unsigned_t<uint64_t>, uint64_t>);

// cv-qualifiers are preserved on forwarded types
static_assert(std::same_as<make_unsigned_t<int const>, std::make_unsigned_t<int const>>);
static_assert(std::same_as<make_unsigned_t<int64_t volatile>, uint64_t volatile>);
static_assert(std::same_as<make_unsigned_t<int16_t const volatile>, uint16_t const volatile>);

// extended to cover unqualified 128-bit types
static_assert(std::same_as<make_unsigned_t<int128_t>, uint128_t>);
static_assert(std::same_as<make_unsigned_t<uint128_t>, uint128_t>);

// cv-qualifiers are preserved on extended types
static_assert(std::same_as<make_unsigned_t<int128_t const>, uint128_t const>);
static_assert(std::same_as<make_unsigned_t<__int128 const volatile>, unsigned __int128 const volatile>);
static_assert(std::same_as<make_unsigned_t<unsigned __int128 volatile>, unsigned __int128 volatile>);

} // namespace make_unsigned_tests

// --------------------------------------------------------------------------------------------------------------------
// sized_integer_t
// --------------------------------------------------------------------------------------------------------------------

namespace sized_integer_tests {

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
[[maybe_unused]] constexpr auto test_all_sizes() noexcept -> void
{
    test_sizes<1, 2, 4, 8, 16>();
}

} // namespace sized_integer_tests

} // namespace
} // namespace crv
