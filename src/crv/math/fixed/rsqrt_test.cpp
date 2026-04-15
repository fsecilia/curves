// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "rsqrt.hpp"
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/fixed/io.hpp>
#include <crv/test/test.hpp>
#include <ostream>

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

using sut_t = quadratic_minimax_t;
using in_t  = sut_t::in_t;
using out_t = sut_t::out_t;

struct rsqrt_initial_guesses_quadratic_minimax_test_t : TestWithParam<in_t>
{
    // This comes from the same sollya script that generated the constants: 2e + e^2 + 100
    static constexpr auto tolerance = out_t::literal(35425386524623839ull);

    in_t const in = GetParam();

    sut_t const sut{};
};

// tests (1/sqrt(in))^2*in = 1
TEST_P(rsqrt_initial_guesses_quadratic_minimax_test_t, error_within_minimax_bounds)
{
    auto const expected = out_t{1};

    auto const reciprocal_sqrt = sut(in);
    auto const reciprocal      = reciprocal_sqrt * reciprocal_sqrt;
    auto const actual          = reciprocal * in;
    auto const difference      = std::max(actual, expected) - std::min(actual, expected);

    EXPECT_LT(difference, tolerance);
};

// clang-format off
constexpr auto epsilon   = in_t::literal(1);
in_t const     vectors[] = {
    in_t::literal(max<uint64_t>()),
    in_t::literal(max<uint64_t>()) - epsilon,
    to_fixed<in_t>(0.75),
    to_fixed<in_t>(0.5) + epsilon,
    to_fixed<in_t>(0.5),
};
INSTANTIATE_TEST_SUITE_P(vectors, rsqrt_initial_guesses_quadratic_minimax_test_t, ValuesIn(vectors));
// clang-format on

} // namespace quadratic_minimax
} // namespace
} // namespace rsqrt_initial_guesses

namespace {

// ====================================================================================================================
// normalized_rsqrt_t
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// property tests
// --------------------------------------------------------------------------------------------------------------------

namespace normalized_rsqrt_property_tests {

using sut_t = normalized_sqrt_t<3, rsqrt_initial_guesses::quadratic_minimax_t>;
using in_t  = sut_t::in_t;
using out_t = fixed_t<uint64_t, 62>;

struct normalized_rsqrt_property_test_t : TestWithParam<in_t>
{
    // This comes from the same sollya script that generated the initial guess constants: test_tolerance for i=3
    static constexpr auto tolerance = out_t::literal(121u);

    in_t const in = GetParam();

    sut_t const sut{};
};

// tests (1/sqrt(in))^2*in = 1
TEST_P(normalized_rsqrt_property_test_t, error_within_minimax_bounds)
{
    auto const expected = out_t{1};

    auto const reciprocal_sqrt = out_t::convert(sut(in));
    auto const reciprocal      = reciprocal_sqrt * reciprocal_sqrt;
    auto const actual          = reciprocal * in;
    auto const difference      = std::max(actual, expected) - std::min(actual, expected);

    EXPECT_LT(difference, tolerance);
};

// clang-format off
constexpr auto epsilon   = in_t::literal(1);
in_t const     vectors[] = {
    in_t::literal(max<uint64_t>()),
    in_t::literal(max<uint64_t>()) - epsilon,
    to_fixed<in_t>(0.75),
    to_fixed<in_t>(0.5) + epsilon,
    to_fixed<in_t>(0.5),
};
INSTANTIATE_TEST_SUITE_P(vectors, normalized_rsqrt_property_test_t, ValuesIn(vectors));
// clang-format on

} // namespace normalized_rsqrt_property_tests

// --------------------------------------------------------------------------------------------------------------------
// value tests
// --------------------------------------------------------------------------------------------------------------------

namespace normalized_rsqrt_value_tests {

using sut_t = normalized_sqrt_t<3, rsqrt_initial_guesses::quadratic_minimax_t>;
using in_t  = sut_t::in_t;
using out_t = fixed_t<uint64_t, 62>;

struct param_t
{
    in_t  in;
    out_t expected;

    friend auto operator<<(std::ostream& out, param_t const& param) -> std::ostream&
    {
        return out << "{.in = " << param.in << ", .expected = " << param.expected << "}";
    }
};

struct normalized_rsqrt_value_test_t : TestWithParam<param_t>
{
    // This comes from the same sollya script that generated the initial guess constants: test_tolerance for i=3
    static constexpr auto tolerance = out_t::literal(11u);

    in_t const  in       = GetParam().in;
    out_t const expected = GetParam().expected;

    sut_t const sut{};
};

// tests (1/sqrt(in))^2*in = 1
TEST_P(normalized_rsqrt_value_test_t, error_within_minimax_bounds)
{
    auto const actual = out_t::convert(sut(in));

    auto const difference = std::max(actual, expected) - std::min(actual, expected);

    EXPECT_LT(difference, tolerance);
};

// clang-format off
param_t const     vectors[] = {
    // generated by sollya:
    // 0.60613487303202367513307659587496906901516058904013 -> 1.2844445620360453097052394113538330209564083350202
    {in_t::literal(11181214876972174247ULL), out_t::literal(5923455028186719836ULL)},
    // 0.85629486703424489697429126637262115781416899421322 -> 1.08065814133809207565273769149180719609537312080573
    {in_t::literal(15795852263811865548ULL), out_t::literal(4983656041108607254ULL)},
    // 0.52719421698353765568260772797146196778464041620012 -> 1.377256025768038300892572217257962389098383593581
    {in_t::literal(9725016797835020697ULL), out_t::literal(6351472357829332510ULL)},
    // 0.72493818328194725726232147113374629916799365663088 -> 1.1744905112338698941268630544046471523435273317842
    {in_t::literal(13372749136262029315ULL), out_t::literal(5416381469432872757ULL)},
    // 0.96029746832506721584280764342561153631634549930545 -> 1.0204626366805731863612962193185662023363601601974
    {in_t::literal(17714361632823719522ULL), out_t::literal(4706053273907346683ULL)},
    // 0.5 -> 1.41421356237309504880168872420969807856967187537694
    {in_t::literal(9223372036854775808ULL), out_t::literal(6521908912666391106ULL)},
    // 0.6 -> 1.29099444873580562839308846659413320361097390176388
    {in_t::literal(11068046444225730970ULL), out_t::literal(5953661049102288004ULL)},
    // 0.7 -> 1.1952286093343936399688171796931249848468790989981
    {in_t::literal(12912720851596686131ULL), out_t::literal(5512019066491833686ULL)},
    // 0.75 -> 1.15470053837925152901829756100391491129520350254026
    {in_t::literal(13835058055282163712ULL), out_t::literal(5325116328314171701ULL)},
    // 0.8 -> 1.11803398874989484820458683436563811772030917980575
    {in_t::literal(14757395258967641293ULL), out_t::literal(5156021714044493573ULL)},
    // 0.9 -> 1.05409255338945977733296451481090617790651837977507
    {in_t::literal(16602069666338596454ULL), out_t::literal(4861143890594596571ULL)},
    // 0.99999999999999999978315956550289911319850943982601 -> 1.00000000000000000010842021724855044341837769534933
    {in_t::literal(18446744073709551612ULL), out_t::literal(4611686018427387905ULL)},
    // 1 / sqrt(2) -> 1.18920711500272106671749997056047591529297209246383
    {in_t::literal(13043817825332782212ULL), out_t::literal(5484249825272419512ULL)},
    // log(2) -> 1.2011224087864497948578032860952217225667640280687
    {in_t::literal(12786308645202655660ULL), out_t::literal(5539199419020296056ULL)},
    // (pi) / 4 -> 1.128379167095512573896158903121545171688101258658
    {in_t::literal(14488038916154245685ULL), out_t::literal(5203730428379116615ULL)},
    // exp(1) / 4 -> 1.21306131942526684720759906998236090688383627097437
    {in_t::literal(12535862302449814171ULL), out_t::literal(5594257926288582650ULL)}
};
INSTANTIATE_TEST_SUITE_P(vectors, normalized_rsqrt_value_test_t, ValuesIn(vectors));
// clang-format on

} // namespace normalized_rsqrt_value_tests

// ====================================================================================================================
// rsqrt_t
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// property tests
// --------------------------------------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------------------------------------
// value tests
// --------------------------------------------------------------------------------------------------------------------

template <is_fixed out_t, is_fixed in_t> constexpr auto rsqrt(in_t in) -> out_t
{
    return rsqrt_t<out_t, in_t>{}(in);
}

template <is_fixed fixed_t> constexpr auto rsqrt(fixed_t in) -> fixed_t
{
    return rsqrt<fixed_t, fixed_t>(in);
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
static_assert(rsqrt(fixed_t<uint64_t, 30>::literal(static_cast<uint64_t>(0.001 * (1ULL << 30))))
              == fixed_t<uint64_t, 30>::literal(33954710857ULL));

// floor
// isqrt(2^63 - 1)
//     ~= 3.2927225399135962335354494746864465592611776064343944463441e-10
//     ~= round(2^60/sqrt(2^63 - 1)) @2^60
static_assert(rsqrt<fixed_t<uint64_t, 60>>(fixed_t<uint64_t, 0>(max<int64_t>()))
              == fixed_t<uint64_t, 60>::literal(379625062ULL));

// overflow saturation
static_assert(rsqrt<fixed_t<uint64_t, 50>>(fixed_t<uint64_t, 30>::literal(1))

              == fixed_t<uint64_t, 50>::literal(max<uint64_t>()));
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
