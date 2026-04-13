// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "rsqrt.hpp"
#include <crv/math/fixed/io.hpp>
#include <crv/test/test.hpp>

namespace crv {
namespace {

using s64              = int64_t;
constexpr auto S64_MAX = max<s64>();

struct ISqrtParam
{
    u64          value;
    unsigned int frac_bits;
    unsigned int output_frac_bits;
    u64          tolerance;
    u64          expected_result;

    friend auto operator<<(std::ostream& out, ISqrtParam const& src) -> std::ostream&
    {
        return out << "{" << src.value << ", " << src.frac_bits << ", " << src.output_frac_bits << ", " << src.tolerance
                   << ", " << src.expected_result << " } ";
    }
};

struct ISqrtTest : TestWithParam<ISqrtParam>
{};

TEST_P(ISqrtTest, expected_result)
{
    auto const expected_result = GetParam().expected_result;
    auto const expected_delta  = GetParam().tolerance;

    auto const actual_result = rsqrt(GetParam().value, GetParam().frac_bits, GetParam().output_frac_bits);

    auto const actual_delta
        = actual_result > expected_result ? actual_result - expected_result : expected_result - actual_result;
    EXPECT_LE(actual_delta, expected_delta)
        << "Input:     " << GetParam().value << "@Q" << GetParam().frac_bits << "\nExpected:  " << expected_result
        << "@Q" << GetParam().output_frac_bits << "\nActual:    " << actual_result << "@Q"
        << GetParam().output_frac_bits << "\nDiff:      " << actual_delta << "\nTolerance: " << expected_delta;
}

ISqrtParam const isqrt_smoke_test[] = {
    // identity
    // isqrt(1.0) = 1.0 = round(2^30) @Q30
    // baseline
    {1LL << 30, 30, 30, 0, 1LL << 30},

    // high precision, under-unity
    // isqrt(2.0) ~= 0.707 ~= round(2^61/sqrt(2)) @Q61
    // fails if internal precision doesn't have guard bits for rne
    {2ll << 61, 61, 61, 148, 1630477228166597777ull},

    // high precision, over-unity
    // isqrt(0.5) ~= 1.414 ~= round(2^61/sqrt(0.5)) @Q61
    // overflow risk case
    {1ll << 60, 61, 61, 295, 3260954456333195553ull},

    // pure integer input
    // isqrt(100) = 0.1 ~= round(2^60/sqrt(100)) @Q60
    // checks standard integer handling and large rescaling
    {100, 0, 60, 1, 115292150460684698ull},

    // irrational non-power-of-2
    // isqrt(3.0) ~= 0.577350269 ~= round(2^60/sqrt(3)) @Q60.
    // checks rounding logic on typical but messy values
    {3ll << 60, 60, 60, 135892519, 665639541039271463ull},

    // large upscale on small input
    // isqrt(0.001*2^30) ~= 0.000965051 ~= round(2^30/sqrt(trunc(0.001*2^30)/2^30)) @Q30
    {static_cast<s64>(0.001 * (1ll << 30)), 30, 30, 0, 33954710857ll},

    // floor
    // isqrt(2^63 - 1)
    //     ~= 3.2927225399135962335354494746864465592611776064343944463441e-10
    //     ~= round(2^60/sqrt(2^63 - 1)) @2^60
    {S64_MAX, 0, 60, 0, 379625062ull},

    // overflow saturation
    {1, 30, 50, U64_MAX, U64_MAX},

    // underflow saturation
    {1ll << 60, 0, 20, 0, 0},

    // max possible mouse input vector
    // round(2^32/sqrt(2*(2^15 - 1)*(2^15 - 1)))
    {2 * ((1ll << 15) - 1) * ((1ll << 15) - 1), 0, 32, 0, 92685},

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
    {2795303719243043561ull, 62, 62, 0, 5923455028186719836ull},
};
INSTANTIATE_TEST_SUITE_P(smoke_tests, ISqrtTest, ValuesIn(isqrt_smoke_test));

} // namespace
} // namespace crv
