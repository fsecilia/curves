// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

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

struct nonsigned_t
{};

// standard signed integer signedness
static_assert(is_signed_v<int8_t>);
static_assert(is_signed_v<int16_t const>);
static_assert(is_signed_v<int32_t volatile>);
static_assert(is_signed_v<int64_t>);
static_assert(is_signed_v<int128_t>);

// standard unsigned integer signedness
static_assert(!is_signed_v<uint8_t>);
static_assert(!is_signed_v<uint16_t const>);
static_assert(!is_signed_v<uint32_t volatile>);
static_assert(!is_signed_v<uint64_t>);
static_assert(!is_signed_v<uint128_t>);

// 128-bit integer signedness
static_assert(is_signed_v<int128_t>);
static_assert(!is_signed_v<uint128_t>);

// arbitrary types are not signed
static_assert(!is_signed_v<nonsigned_t>);

// signed integers are signed integrals
static_assert(signed_integral<int8_t>);
static_assert(signed_integral<int16_t const>);
static_assert(signed_integral<int32_t volatile>);
static_assert(signed_integral<int64_t>);
static_assert(signed_integral<int128_t>);

// unsigned integers are not signed integrals
static_assert(!signed_integral<uint8_t>);
static_assert(!signed_integral<uint16_t const>);
static_assert(!signed_integral<uint32_t volatile>);
static_assert(!signed_integral<uint64_t>);
static_assert(!signed_integral<uint128_t>);

// floating point types are not signed integrals
static_assert(!signed_integral<float>);
static_assert(!signed_integral<double>);
static_assert(!signed_integral<long double>);

// arbitrary types are not signed integrals
static_assert(!signed_integral<nonsigned_t>);

// signed integers are not unsigned integrals
static_assert(!unsigned_integral<int8_t>);
static_assert(!unsigned_integral<int16_t const>);
static_assert(!unsigned_integral<int32_t volatile>);
static_assert(!unsigned_integral<int64_t>);
static_assert(!unsigned_integral<int128_t>);

// unsigned integers are unsigned integrals
static_assert(unsigned_integral<uint8_t>);
static_assert(unsigned_integral<uint16_t const>);
static_assert(unsigned_integral<uint32_t volatile>);
static_assert(unsigned_integral<uint64_t>);
static_assert(unsigned_integral<uint128_t>);

// floating point types are not unsigned integrals
static_assert(!unsigned_integral<float>);
static_assert(!unsigned_integral<double>);
static_assert(!unsigned_integral<long double>);

// arbitrary types are not unsigned integrals
static_assert(!unsigned_integral<nonsigned_t>);

} // namespace is_signed_tests

// --------------------------------------------------------------------------------------------------------------------
// make_signed
// --------------------------------------------------------------------------------------------------------------------

namespace make_signed_tests {

// basic types match std::make_signed
static_assert(std::same_as<make_signed_t<int>, std::make_signed_t<int>>);
static_assert(std::same_as<make_signed_t<signed>, std::make_signed_t<signed>>);
static_assert(std::same_as<make_signed_t<signed int>, std::make_signed_t<signed int>>);
static_assert(std::same_as<make_signed_t<unsigned>, std::make_signed_t<unsigned>>);
static_assert(std::same_as<make_signed_t<unsigned int>, std::make_signed_t<unsigned int>>);

// sized_types match std::make_signed
static_assert(std::same_as<make_signed_t<int8_t>, int8_t>);
static_assert(std::same_as<make_signed_t<int64_t>, int64_t>);
static_assert(std::same_as<make_signed_t<uint8_t>, int8_t>);
static_assert(std::same_as<make_signed_t<uint64_t>, int64_t>);

// cv-qualifiers are preserved on forwarded types
static_assert(std::same_as<make_signed_t<unsigned const>, std::make_signed_t<int const>>);
static_assert(std::same_as<make_signed_t<uint16_t const volatile>, int16_t const volatile>);
static_assert(std::same_as<make_signed_t<uint64_t volatile>, int64_t volatile>);

// extended to cover unqualified 128-bit types
static_assert(std::same_as<make_signed_t<int128_t>, int128_t>);
static_assert(std::same_as<make_signed_t<uint128_t>, int128_t>);

// cv-qualifiers are preserved on extended types
static_assert(std::same_as<make_signed_t<int128_t const>, int128_t const>);
static_assert(std::same_as<make_signed_t<uint128_t const>, int128_t const>);
static_assert(std::same_as<make_signed_t<__int128 const volatile>, __int128 const volatile>);
static_assert(std::same_as<make_signed_t<signed __int128 volatile>, signed __int128 volatile>);
static_assert(std::same_as<make_signed_t<unsigned __int128 volatile>, signed __int128 volatile>);

} // namespace make_signed_tests

// --------------------------------------------------------------------------------------------------------------------
// make_unsigned
// --------------------------------------------------------------------------------------------------------------------

namespace make_unsigned_tests {

// basic types match std::make_unsigned
static_assert(std::same_as<make_unsigned_t<int>, std::make_unsigned_t<int>>);
static_assert(std::same_as<make_unsigned_t<signed>, std::make_unsigned_t<signed>>);
static_assert(std::same_as<make_unsigned_t<signed int>, std::make_unsigned_t<signed int>>);
static_assert(std::same_as<make_unsigned_t<unsigned>, std::make_unsigned_t<unsigned>>);
static_assert(std::same_as<make_unsigned_t<unsigned int>, std::make_unsigned_t<unsigned int>>);

// sized_types match std::make_unsigned
static_assert(std::same_as<make_unsigned_t<int8_t>, uint8_t>);
static_assert(std::same_as<make_unsigned_t<int64_t>, uint64_t>);
static_assert(std::same_as<make_unsigned_t<uint8_t>, uint8_t>);
static_assert(std::same_as<make_unsigned_t<uint64_t>, uint64_t>);

// cv-qualifiers are preserved on forwarded types
static_assert(std::same_as<make_unsigned_t<int const>, std::make_unsigned_t<int const>>);
static_assert(std::same_as<make_unsigned_t<int16_t const volatile>, uint16_t const volatile>);
static_assert(std::same_as<make_unsigned_t<int64_t volatile>, uint64_t volatile>);

// extended to cover unqualified 128-bit types
static_assert(std::same_as<make_unsigned_t<int128_t>, uint128_t>);
static_assert(std::same_as<make_unsigned_t<uint128_t>, uint128_t>);

// cv-qualifiers are preserved on extended types
static_assert(std::same_as<make_unsigned_t<int128_t const>, uint128_t const>);
static_assert(std::same_as<make_unsigned_t<uint128_t const>, uint128_t const>);
static_assert(std::same_as<make_unsigned_t<__int128 const volatile>, unsigned __int128 const volatile>);
static_assert(std::same_as<make_unsigned_t<signed __int128 volatile>, unsigned __int128 volatile>);
static_assert(std::same_as<make_unsigned_t<unsigned __int128 volatile>, unsigned __int128 volatile>);

} // namespace make_unsigned_tests

// --------------------------------------------------------------------------------------------------------------------
// Sized Integers
// --------------------------------------------------------------------------------------------------------------------

namespace int_by_bytes_tests {

static_assert(std::same_as<int8_t, int_by_bytes_t<1, true>>);
static_assert(std::same_as<int16_t, int_by_bytes_t<2, true>>);
static_assert(std::same_as<int32_t, int_by_bytes_t<4, true>>);
static_assert(std::same_as<int64_t, int_by_bytes_t<8, true>>);
static_assert(std::same_as<int128_t, int_by_bytes_t<16, true>>);

static_assert(std::same_as<uint8_t, int_by_bytes_t<1, false>>);
static_assert(std::same_as<uint16_t, int_by_bytes_t<2, false>>);
static_assert(std::same_as<uint32_t, int_by_bytes_t<4, false>>);
static_assert(std::same_as<uint64_t, int_by_bytes_t<8, false>>);
static_assert(std::same_as<uint128_t, int_by_bytes_t<16, false>>);

} // namespace int_by_bytes_tests

namespace int_by_bits_tests {

static_assert(std::same_as<int8_t, int_by_bits_t<1, true>>);
static_assert(std::same_as<int8_t, int_by_bits_t<8, true>>);
static_assert(std::same_as<int16_t, int_by_bits_t<9, true>>);
static_assert(std::same_as<int16_t, int_by_bits_t<16, true>>);
static_assert(std::same_as<int32_t, int_by_bits_t<17, true>>);
static_assert(std::same_as<int32_t, int_by_bits_t<32, true>>);
static_assert(std::same_as<int64_t, int_by_bits_t<33, true>>);
static_assert(std::same_as<int64_t, int_by_bits_t<64, true>>);
static_assert(std::same_as<int128_t, int_by_bits_t<65, true>>);
static_assert(std::same_as<int128_t, int_by_bits_t<128, true>>);

static_assert(std::same_as<uint8_t, int_by_bits_t<1, false>>);
static_assert(std::same_as<uint8_t, int_by_bits_t<8, false>>);
static_assert(std::same_as<uint16_t, int_by_bits_t<9, false>>);
static_assert(std::same_as<uint16_t, int_by_bits_t<16, false>>);
static_assert(std::same_as<uint32_t, int_by_bits_t<17, false>>);
static_assert(std::same_as<uint32_t, int_by_bits_t<32, false>>);
static_assert(std::same_as<uint64_t, int_by_bits_t<33, false>>);
static_assert(std::same_as<uint64_t, int_by_bits_t<64, false>>);
static_assert(std::same_as<uint128_t, int_by_bits_t<65, false>>);
static_assert(std::same_as<uint128_t, int_by_bits_t<128, false>>);

} // namespace int_by_bits_tests

// --------------------------------------------------------------------------------------------------------------------
// Integer Promotions
// --------------------------------------------------------------------------------------------------------------------

namespace integer_promotion_tests {

static_assert(std::same_as<widened_t<int8_t>, int16_t>);
static_assert(std::same_as<widened_t<int16_t>, int32_t>);
static_assert(std::same_as<widened_t<int32_t>, int64_t>);
static_assert(std::same_as<widened_t<int64_t>, int128_t>);

static_assert(std::same_as<widened_t<uint8_t>, uint16_t>);
static_assert(std::same_as<widened_t<uint16_t>, uint32_t>);
static_assert(std::same_as<widened_t<uint32_t>, uint64_t>);
static_assert(std::same_as<widened_t<uint64_t>, uint128_t>);

} // namespace integer_promotion_tests

} // namespace
} // namespace crv
