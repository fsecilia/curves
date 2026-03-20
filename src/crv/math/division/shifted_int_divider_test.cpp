// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "shifted_int_divider.hpp"
#include <crv/math/division/result.hpp>
#include <crv/test/test.hpp>

namespace crv::division {
namespace {

// ====================================================================================================================
// Test Doubles
// ====================================================================================================================

/// stub; returns compile-time constant regardless of inputs
///
/// This aims the prescaling divider's output at exact saturation boundaries.
template <typename wide_t, wide_t canned_value> struct constant_divider_t
{
    template <typename... args_t> constexpr auto operator()(args_t...) const noexcept -> wide_t { return canned_value; }
};

/// fake; returns an asymmetric combination of inputs so tests can detect argument swaps
///
/// The weights here, (3, 2), are arbitrary primes; they just need to be distinct so f(a, b) != f(b, a).
template <typename wide_t, typename narrow_t, typename rounding_mode_t> struct forwarding_divider_t
{
    constexpr auto operator()(narrow_t dividend, narrow_t divisor, rounding_mode_t rounding_mode) const noexcept
        -> wide_t
    {
        return int_cast<wide_t>(dividend) * 3 + int_cast<wide_t>(divisor) * 2 + int_cast<wide_t>(rounding_mode);
    }
};

/// dummy; rounding mode is just an int
///
/// Normally, rounding mode is a type with methods, but we just need to verify that it is fowarded, and any value type
/// is sufficient for that.
using rounding_mode_t        = int_t;
constexpr auto rounding_mode = int_t{7};

// ====================================================================================================================
// Unsigned
// ====================================================================================================================

namespace unsigned_tests {

using narrow_t = uint8_t;
using wide_t   = uint16_t;

constexpr auto dividend = narrow_t{11};
constexpr auto divisor  = narrow_t{13};

// --------------------------------------------------------------------------------------------------------------------
// Forwarding
// --------------------------------------------------------------------------------------------------------------------

using forwarding_divider_t = forwarding_divider_t<wide_t, narrow_t, rounding_mode_t>;
using forwarding_sut_t     = shifted_int_divider_t<forwarding_divider_t, true>;

// 11 * 3 + 13 * 2 + 7 = 66, fits in narrow_t, so no saturation.
// Swapped arguments would give 13 * 3 + 11 * 2 + 7 = 68
static_assert(forwarding_sut_t{}(dividend, divisor, rounding_mode) == narrow_t{66});

// --------------------------------------------------------------------------------------------------------------------
// Saturation
// --------------------------------------------------------------------------------------------------------------------

template <wide_t canned_value, bool saturates>
using constant_sut_t = shifted_int_divider_t<constant_divider_t<wide_t, canned_value>, saturates>;

// boundary, exactly max: representable, passes through regardless of saturation policy
static_assert(constant_sut_t<wide_t{255}, true>{}(dividend, divisor, rounding_mode) == max<narrow_t>());
static_assert(constant_sut_t<wide_t{255}, false>{}(dividend, divisor, rounding_mode) == max<narrow_t>());

// boundary, first non-representable value
static_assert(constant_sut_t<wide_t{256}, true>{}(dividend, divisor, rounding_mode) == max<narrow_t>()); // saturates
static_assert(constant_sut_t<wide_t{256}, false>{}(dividend, divisor, rounding_mode) == narrow_t{0});    // truncates

// deep out of range sanity check
static_assert(constant_sut_t<wide_t{1000}, true>{}(dividend, divisor, rounding_mode) == max<narrow_t>());

// --------------------------------------------------------------------------------------------------------------------
// Width Coverage
// --------------------------------------------------------------------------------------------------------------------

template <typename narrow_t> struct unsigned_boundary_check_t
{
    using wide_t            = wider_t<narrow_t>;
    static constexpr auto d = narrow_t{1};

    template <wide_t canned_value, bool saturates>
    using constant_sut_t = shifted_int_divider_t<constant_divider_t<wide_t, canned_value>, saturates>;

    static_assert(constant_sut_t<wide_t{max<narrow_t>()}, true>{}(d, d, rounding_mode) == max<narrow_t>());
    static_assert(constant_sut_t<wide_t{max<narrow_t>()} + 1, true>{}(d, d, rounding_mode) == max<narrow_t>());
    static_assert(constant_sut_t<wide_t{max<narrow_t>()} + 1, false>{}(d, d, rounding_mode) == narrow_t{0});
};

template struct unsigned_boundary_check_t<uint8_t>;
template struct unsigned_boundary_check_t<uint16_t>;
template struct unsigned_boundary_check_t<uint32_t>;

} // namespace unsigned_tests

// ====================================================================================================================
// Signed
// ====================================================================================================================

namespace signed_tests {

using narrow_t   = int8_t;
using unsigned_t = uint8_t;
using wide_t     = uint16_t;

constexpr auto positive_dividend = narrow_t{11};
constexpr auto negative_dividend = narrow_t{-11};
constexpr auto positive_divisor  = narrow_t{13};
constexpr auto negative_divisor  = narrow_t{-13};

// --------------------------------------------------------------------------------------------------------------------
// Forwarding
// --------------------------------------------------------------------------------------------------------------------

// forwarding divider receives unsigned_t args from the signed overload
using forwarding_divider_t = forwarding_divider_t<wide_t, unsigned_t, rounding_mode_t>;
using forwarding_sut_t     = shifted_int_divider_t<forwarding_divider_t, true>;

// All four sign quadrants produce same magnitude: 11 * 3 + 13 * 2 + 7 = 66
// Only the final sign differs. If either abs were missing, the asymmetric combination would produce a different value.
static_assert(forwarding_sut_t{}(positive_dividend, positive_divisor, rounding_mode) == narrow_t{66});  // +/+ -> +
static_assert(forwarding_sut_t{}(positive_dividend, negative_divisor, rounding_mode) == narrow_t{-66}); // +/- -> -
static_assert(forwarding_sut_t{}(negative_dividend, positive_divisor, rounding_mode) == narrow_t{-66}); // -/+ -> -
static_assert(forwarding_sut_t{}(negative_dividend, negative_divisor, rounding_mode) == narrow_t{66});  // -/- -> +

// --------------------------------------------------------------------------------------------------------------------
// Positive Saturation Boundaries
//
// bound = max<int8_t>() = 127
// --------------------------------------------------------------------------------------------------------------------

template <wide_t v, bool sat> using constant_sut_t = shifted_int_divider_t<constant_divider_t<wide_t, v>, sat>;

// magnitude 127: exactly representable as +127
static_assert(constant_sut_t<127, true>{}(positive_dividend, positive_divisor, rounding_mode) == narrow_t{127});

// magnitude 128: not representable as positive, must saturate or truncate
static_assert(constant_sut_t<128, true>{}(positive_dividend, positive_divisor, rounding_mode) == max<narrow_t>());
static_assert(constant_sut_t<128, false>{}(positive_dividend, positive_divisor, rounding_mode) == narrow_t{-128});

// --------------------------------------------------------------------------------------------------------------------
// Negative Saturation Boundaries
//
// bound = max<int8_t>() + 1 = 128
// --------------------------------------------------------------------------------------------------------------------

// magnitude 128: exactly representable as -128
// This is the asymmetric two's complement case — the most important test in this module. It must not saturate.
static_assert(constant_sut_t<128, true>{}(negative_dividend, positive_divisor, rounding_mode) == min<narrow_t>());

// magnitude 129: exceeds negative bound, must saturate or truncate
static_assert(constant_sut_t<129, true>{}(negative_dividend, positive_divisor, rounding_mode) == min<narrow_t>());
static_assert(constant_sut_t<129, false>{}(negative_dividend, positive_divisor, rounding_mode) == narrow_t{127});

// --------------------------------------------------------------------------------------------------------------------
// Edge-Cases
// --------------------------------------------------------------------------------------------------------------------

// The constant divider here returns an arbitary, small, in-range value so saturation never triggers.
// This tests the input-side abs/sign logic, not the output-side clamping.
constexpr auto edge_const = narrow_t{31};
using edge_case_sut_t     = shifted_int_divider_t<constant_divider_t<wide_t, edge_const>, true>;

// min dividend
static_assert(edge_case_sut_t{}(min<narrow_t>(), narrow_t{1}, rounding_mode) == -edge_const);

// min divisor
static_assert(edge_case_sut_t{}(narrow_t{-1}, min<narrow_t>(), rounding_mode) == edge_const);

// both min
static_assert(edge_case_sut_t{}(min<narrow_t>(), min<narrow_t>(), rounding_mode) == edge_const);

// Zero dividend with positive and negative divisor.
// The constant divider receives abs(0) = 0 regardless, but the fake ignores inputs and returns edge_const. Sign is
// determined by (0 ^ divisor) < 0.
static_assert(edge_case_sut_t{}(narrow_t{0}, narrow_t{1}, rounding_mode) == narrow_t{edge_const});
static_assert(edge_case_sut_t{}(narrow_t{0}, narrow_t{-1}, rounding_mode) == narrow_t{-edge_const});

// --------------------------------------------------------------------------------------------------------------------
// Width Coverage
// --------------------------------------------------------------------------------------------------------------------

template <typename narrow_t> struct signed_boundary_check_t
{
    using unsigned_t          = make_unsigned_t<narrow_t>;
    using wide_t              = wider_t<unsigned_t>;
    static constexpr auto pos = narrow_t{1};
    static constexpr auto neg = narrow_t{-1};

    template <wide_t canned_value, bool saturates>
    using constant_sut_t = shifted_int_divider_t<constant_divider_t<wide_t, canned_value>, saturates>;

    // positive result: max<narrow_t>() is the bound
    static_assert(constant_sut_t<wide_t(max<narrow_t>()), true>{}(pos, pos, rounding_mode) == max<narrow_t>());
    static_assert(constant_sut_t<wide_t(max<narrow_t>()) + 1, true>{}(pos, pos, rounding_mode) == max<narrow_t>());

    // negative result: max<narrow_t>() + 1 is representable as min<narrow_t>()
    static_assert(constant_sut_t<wide_t(max<narrow_t>()) + 1, true>{}(neg, pos, rounding_mode) == min<narrow_t>());
    static_assert(constant_sut_t<wide_t(max<narrow_t>()) + 2, true>{}(neg, pos, rounding_mode) == min<narrow_t>());
};

template struct signed_boundary_check_t<int8_t>;
template struct signed_boundary_check_t<int16_t>;
template struct signed_boundary_check_t<int32_t>;

} // namespace signed_tests

} // namespace
} // namespace crv::division
