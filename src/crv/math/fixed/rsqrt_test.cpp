// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "rsqrt.hpp"
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/fixed/io.hpp>
#include <crv/test/test.hpp>

namespace crv {

using real_t = float_t;

// ====================================================================================================================
// initial guesses
// ====================================================================================================================

namespace rsqrt_initial_guesses {
namespace {

// --------------------------------------------------------------------------------------------------------------------
// quadratic minimiax
// --------------------------------------------------------------------------------------------------------------------

namespace quadratic_minimax {

using coeff_t = quadratic_minimax_t::coeff_t;

struct rsqrt_initial_guesses_quadratic_minimax_test_t : TestWithParam<coeff_t>
{
    // This comes from the same sollya script that generated the constants: 2e + e^2 + 100
    static constexpr auto tolerance = coeff_t::literal(35425386524623938ull);

    coeff_t const in = GetParam();

    using sut_t = quadratic_minimax_t;
    sut_t const sut{};
};

// tests (1/sqrt(in))^2*in = 1
TEST_P(rsqrt_initial_guesses_quadratic_minimax_test_t, error_within_minimax_bounds)
{
    auto const expected = coeff_t{1};

    auto const reciprocal_sqrt = sut(in);
    auto const reciprocal      = reciprocal_sqrt * reciprocal_sqrt;
    auto const actual          = reciprocal * in;
    auto const difference      = std::max(actual, expected) - std::min(actual, expected);

    EXPECT_LT(difference, tolerance);
};

// clang-format off
constexpr auto epsilon   = coeff_t::literal(1);
coeff_t const  vectors[] = {
    {to_fixed<coeff_t>(1.0) - epsilon},
    {to_fixed<coeff_t>(1.0) - epsilon * 2},
    {to_fixed<coeff_t>(0.75)},
    {to_fixed<coeff_t>(0.5) + epsilon},
    {to_fixed<coeff_t>(0.5)},
};
INSTANTIATE_TEST_SUITE_P(vectors, rsqrt_initial_guesses_quadratic_minimax_test_t, ValuesIn(vectors));
// clang-format on

} // namespace quadratic_minimax
} // namespace
} // namespace rsqrt_initial_guesses

// ====================================================================================================================
// rsqrt
// ====================================================================================================================

namespace {

template <unsigned int output_frac_bits, unsigned int frac_bits>
constexpr auto test(u64 value, u64 tolerance, u64 expected_result) -> bool
{
    auto const actual_result = rsqrt<output_frac_bits, frac_bits>(value);

    auto const actual_delta
        = actual_result > expected_result ? actual_result - expected_result : expected_result - actual_result;
    return actual_delta <= tolerance;
}

// identity
// isqrt(1.0) = 1.0 = round(2^30) @Q30
// baseline
static_assert(rsqrt(fixed_t<uint64_t, 30>(1)) == fixed_t<uint64_t, 30>(1));

// high precision, under-unity
// isqrt(2.0) ~= 0.707 ~= round(2^61/sqrt(2)) @Q61
// fails if internal precision doesn't have guard bits for rne
static_assert(rsqrt(fixed_t<uint64_t, 61>(2)) == fixed_t<uint64_t, 61>::literal(1630477228166597777ULL - 1));

// high precision, over-unity
// isqrt(0.5) ~= 1.414 ~= round(2^61/sqrt(0.5)) @Q61
// overflow risk case
static_assert(rsqrt((fixed_t<uint64_t, 61>(1) >> 1)) == fixed_t<uint64_t, 61>::literal(3260954456333195553ULL - 1));

// pure integer input
// isqrt(100) = 0.1 ~= round(2^60/sqrt(100)) @Q60
// checks standard integer handling and large rescaling
static_assert(rsqrt<fixed_t<uint64_t, 60>>(fixed_t<uint64_t, 0>(100))
              == fixed_t<uint64_t, 60>::literal(115292150460684698ULL));

// irrational non-power-of-2
// isqrt(3.0) ~= 0.577350269 ~= round(2^60/sqrt(3)) @Q60.
// checks rounding logic on typical but messy values
static_assert(rsqrt(fixed_t<uint64_t, 60>(3)) == fixed_t<uint64_t, 60>::literal(665639541039271463ULL));

// large upscale on small input
// isqrt(0.001*2^30) ~= 0.000965051 ~= round(2^30/sqrt(trunc(0.001*2^30)/2^30)) @Q30
static_assert(rsqrt(fixed_t<uint64_t, 30>::literal(static_cast<u64>(0.001 * (1ULL << 30))))
              == fixed_t<uint64_t, 30>::literal(33954710857ULL));

// floor
// isqrt(2^63 - 1)
//     ~= 3.2927225399135962335354494746864465592611776064343944463441e-10
//     ~= round(2^60/sqrt(2^63 - 1)) @2^60
static_assert(rsqrt<fixed_t<uint64_t, 60>>(fixed_t<uint64_t, 0>(S64_MAX))
              == fixed_t<uint64_t, 60>::literal(379625062ULL));

// overflow saturation
static_assert(rsqrt<fixed_t<uint64_t, 50>>(fixed_t<uint64_t, 30>::literal(1))

              == fixed_t<uint64_t, 50>::literal(U64_MAX));
// underflow saturation
static_assert(rsqrt<fixed_t<uint64_t, 20>>(fixed_t<uint64_t, 0>(1ULL << 60)) == fixed_t<uint64_t, 20>{0});

// max possible mouse input vector
// round(2^32/sqrt(2*(2^15 - 1)*(2^15 - 1)))
static_assert(rsqrt<fixed_t<uint64_t, 32>>(fixed_t<uint64_t, 0>::literal(2 * ((1ULL << 15) - 1) * ((1ULL << 15) - 1)))
              == fixed_t<uint64_t, 32>::literal(92685ULL));

// worst initial guess
//
// The sollya script outputs the locations of the worst initial guesses. These are the inputs that cause the initial
// guess to be the farthest from the actual solution, requiring the algorithm to bridge the largest gap possible via
// NR steps alone. There are 2 locations. This test arbitrarily chooses the first:
// x ~= 0.60613487303202367513307659587496906901516058904013
//   ~= trunc(0.60613487303202367513307659587496906901516058904013*2^62)
//   ~= 2795303719243043561 @Q62
//
// The expected output is:
// 1.0/sqrt(0.60613487303202367513307659587496906901516058904013)
//  ~= 1.2844445620360453097052394113538330209564083350201993080566792025
//  ~= round(2^62/sqrt(0.60613487303202367513307659587496906901516058904013))
//  ~= 5923455028186719836 @Q62
//
// The initial guess is:
// 10354071711462988194
//     - trunc(
//          2795303719243043561*(9674659108971248202 - trunc(2795303719243043561*3949952137299739940/2^62))/2^62
//       )
// ~= 10354071711462988194 - trunc(2795303719243043561*(9674659108971248202 - 2394203737224748414)/2^62)
// ~= 10354071711462988194 - trunc(2795303719243043561*7280455371746499788/2^62)
// ~= 10354071711462988194 - 4412937892368879373
// ~= 5941133819094108821 @q62
// ~= 1.28827803873778693366687619903387940212269313633441925048828125
//
// It starts with a guess of 1.2883 and must reach 1.2844 in NR steps alone.
static_assert(rsqrt(fixed_t<uint64_t, 62>::literal(2795303719243043561ULL))
              == fixed_t<uint64_t, 62>::literal(5923455028186719836ULL));

} // namespace
} // namespace crv
