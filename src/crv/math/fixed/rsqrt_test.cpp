// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "rsqrt.hpp"
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/fixed/io.hpp>
#include <crv/test/test.hpp>
#include <ostream>
#include <random>

namespace crv {

using real_t = float_t;

static constexpr auto e_nr = uint128_t{14}; // error calculated after 3 iterations of Newton-Raphson, comes from sollya

// ====================================================================================================================
// initial guesses
// ====================================================================================================================

namespace rsqrt_initial_guesses {
namespace {

// --------------------------------------------------------------------------------------------------------------------
// quadratic minimiax
// --------------------------------------------------------------------------------------------------------------------

namespace quadratic_minimax_test {

using sut_t = quadratic_minimax_t;
using in_t = sut_t::in_t;
using out_t = sut_t::out_t;

struct rsqrt_initial_guesses_quadratic_minimax_test_t : TestWithParam<in_t>
{
    // this comes from the same sollya script that generated the constants: 2e + e^2 + 100
    static constexpr auto tolerance = out_t::literal(35425316374295423ULL);

    in_t const x = GetParam();

    sut_t const sut{};
};

// tests (1/sqrt(in))^2*in = 1
TEST_P(rsqrt_initial_guesses_quadratic_minimax_test_t, error_within_minimax_bounds)
{
    auto const expected = out_t{1};

    auto const y = sut(x);
    auto const yy = y * y;
    auto const actual = yy * x;
    auto const difference = std::max(actual, expected) - std::min(actual, expected);

    EXPECT_LT(difference, tolerance);
};

constexpr auto epsilon = in_t::literal(1);
in_t const vectors[] = {
    in_t::literal(max<uint64_t>()),
    in_t::literal(max<uint64_t>() - 1),
    to_fixed<in_t>(0.75) + epsilon,
    to_fixed<in_t>(0.75),
    to_fixed<in_t>(0.75) - epsilon,
    to_fixed<in_t>(0.5) + epsilon,
    to_fixed<in_t>(0.5),
};
INSTANTIATE_TEST_SUITE_P(vectors, rsqrt_initial_guesses_quadratic_minimax_test_t, ValuesIn(vectors));

} // namespace quadratic_minimax_test
} // namespace
} // namespace rsqrt_initial_guesses

namespace {

// ====================================================================================================================
// normalized_rsqrt_t
// ====================================================================================================================

namespace normalized_rsqrt_test {

// --------------------------------------------------------------------------------------------------------------------
// property tests
// --------------------------------------------------------------------------------------------------------------------

namespace property_test {

using sut_t = normalized_rsqrt_t<>;
using in_t = sut_t::in_t;
using out_t = fixed_t<uint64_t, 62>;

struct normalized_rsqrt_property_test_t : TestWithParam<in_t>
{
    // this comes from the same sollya script that generated the initial guess constants: test_tolerance for i=3
    static constexpr auto tolerance = out_t::literal(e_nr * 2);

    in_t const x = GetParam();

    sut_t const sut{};
};

// tests (1/sqrt(in))^2*in = 1
TEST_P(normalized_rsqrt_property_test_t, error_within_minimax_bounds)
{
    auto const expected = out_t{1};

    auto const y = out_t::convert(sut(x));
    auto const yy = y * y;
    auto const actual = yy * x;
    auto const difference = std::max(actual, expected) - std::min(actual, expected);

    EXPECT_LT(difference, tolerance);
};

constexpr auto epsilon = in_t::literal(1);
in_t const vectors[] = {
    in_t::literal(max<uint64_t>()),
    in_t::literal(max<uint64_t>() - 1),
    to_fixed<in_t>(0.75) + epsilon,
    to_fixed<in_t>(0.75),
    to_fixed<in_t>(0.75) - epsilon,
    to_fixed<in_t>(0.5) + epsilon,
    to_fixed<in_t>(0.5),
};
INSTANTIATE_TEST_SUITE_P(vectors, normalized_rsqrt_property_test_t, ValuesIn(vectors));

} // namespace property_test

// --------------------------------------------------------------------------------------------------------------------
// value tests
// --------------------------------------------------------------------------------------------------------------------

namespace value_test {

using sut_t = normalized_rsqrt_t<3, rsqrt_initial_guesses::quadratic_minimax_t>;
using in_t = sut_t::in_t;
using out_t = fixed_t<uint64_t, 62>;

struct param_t
{
    in_t x;
    out_t expected;

    friend auto operator<<(std::ostream& out, param_t const& param) -> std::ostream&
    {
        return out << "{.in = " << param.x << ", .expected = " << param.expected << "}";
    }
};

struct normalized_rsqrt_value_test_t : TestWithParam<param_t>
{
    static constexpr auto tolerance = out_t::literal(e_nr);

    in_t const x = GetParam().x;
    out_t const expected = GetParam().expected;

    sut_t const sut{};
};

// tests (1/sqrt(in))^2*in = 1
TEST_P(normalized_rsqrt_value_test_t, error_within_minimax_bounds)
{
    auto const actual = out_t::convert(sut(x));

    auto const difference = std::max(actual, expected) - std::min(actual, expected);

    EXPECT_LE(difference, tolerance);
};

// clang-format off
param_t const vectors[] = {
    // generated by sollya:
    // 0.60613493946432108055646607122248333065308120072573978658283021277485080093673337960449987432858717759851930652111571009758810320043143523389129267855440486 -> 1.2844444916485788163793996641593715147224007703598
    {in_t::literal(11181216102431762714ULL), out_t::literal(5923454703581824736ULL)},
    // 0.8562948138918169308095258234744331138858581621929563350175220989192965782703772325934318070888743745438834811889875136211257453417281098142605047237260169 -> 1.0806581748714007033655217322069395282042692558206
    {in_t::literal(15795851283507097401ULL), out_t::literal(4983656195753697804ULL)},
    // 0.5271942989291745876854619998738462450847567629828235393050508415334503339984035384549005510886920499878338120063895307018656314888611404032737954828523153 -> 1.37725591872958315740259922005936731740838182742294
    {in_t::literal(9725018309465213139ULL), out_t::literal(6351471864201585490ULL)},
    // 0.72493823475702670199680512789725657044125826443091998709051504193275523388618624903894024250740146351198918602411882278839422469369211710667204359521973774 -> 1.17449046953583925685607106762438375896496096824266
    {in_t::literal(13372750085809646006ULL), out_t::literal(5416381277134647871ULL)},
    // 0.960297336721575074889681862971413218184209428272096692836670752088587757577964779252204286035151273387194203094573207329239287865577423223747514010506187 -> 1.02046270660497933116228598243127001183290398864588
    {in_t::literal(17714359205167780791ULL), out_t::literal(4706053596376752848ULL)},
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
    {in_t::literal(12535862302449814171ULL), out_t::literal(5594257926288582650ULL)},
};
INSTANTIATE_TEST_SUITE_P(vectors, normalized_rsqrt_value_test_t, ValuesIn(vectors));
// clang-format on

} // namespace value_test
} // namespace normalized_rsqrt_test

// ====================================================================================================================
// rsqrt_t
// ====================================================================================================================

namespace rsqrt_test {

// --------------------------------------------------------------------------------------------------------------------
// property tests
// --------------------------------------------------------------------------------------------------------------------

namespace property_test {

// test using pipeline case: mouse vector in 32 bit container -> 8 integer bits in 64-bit container
using in_t = fixed_t<uint32_t, 0>;
using out_t = fixed_t<uint64_t, 56>;

struct rsqrt_property_test_t : Test
{
    using in_value_t = in_t::value_t;
    using wide_out_t = fixed_t<widened_t<out_t::value_t>, out_t::frac_bits * 2>;
    using sut_t = rsqrt_t<out_t, in_t, normalized_rsqrt_t<>>;

    sut_t const sut{};

    // tests (1/sqrt(x))^2*x = 1
    //
    // This test computes y^2*x with wide intermediates to avoid arithmetic precision loss native to the test.
    // Straightforward operator* narrows y^2 back to Q56 before multiplying by x, which discards fractional bits that
    // would have contributed to the final product. For large x this truncation dominates the result, masking the actual
    // rsqrt error. Instead, we crack fixed_t and multiply directly. multiply() keeps y^2 at full Q112 width, and we
    // manually compute yy.value * x.value in uint128. This is abusive, but safe because y^2*x ~= 1, so the raw product
    // stays near 2^112, well within uint128_t.
    //
    // The tolerance is input-dependent, but in a specific way that comes from the math. Expanding y = 1/sqrt(x) + d
    // around the true root:
    //
    //     y^2*x = 1 + 2*d*sqrt(x) + d^2*x
    //
    // The dominant error term is 2*d*sqrt(x), giving a tolerance of 2*e_nr*sqrt(x) ulps of Q56. We approximate
    // sqrt(x)*2^56 ~= x.value * y.value (since y ~= 1/sqrt(x) in Q56). e_nr is the max error after 3 NR iterations
    // from the sollya script (see rsqrt.hpp). Denormalization only reduces it (right-shift for integer Q0 inputs).
    auto test_property(in_t x) const noexcept -> void
    {
        auto const expected = wide_out_t{1};

        auto const y = out_t::convert(sut(x));
        auto const yy = multiply(y, y);

        auto const actual = wide_out_t::literal(yy.value * uint128_t{x.value});
        auto const difference = std::max(actual, expected) - std::min(actual, expected);
        auto const tolerance = wide_out_t::literal(2 * e_nr * uint128_t{x.value} * uint128_t{y.value});

        EXPECT_LT(difference, tolerance);
    }
};

TEST_F(rsqrt_property_test_t, fuzz)
{
    std::mt19937 rng{0xF012345678};
    auto literal_value_distribution = std::uniform_int_distribution<in_value_t>{0, max<in_value_t>()};

    for (auto i = 0; i < 10000; ++i) test_property(in_t::literal(literal_value_distribution(rng)));
}

// sweeping test for specific ranges
struct rsqrt_property_test_sweep_t : rsqrt_property_test_t
{
    static auto const sample_count = in_value_t{10000};

    // sweeps [range_begin, range_begin + sample_count) densely
    auto sweep_range(in_t range_begin) const noexcept -> void
    {
        for (auto sample = in_value_t{}; sample < sample_count; ++sample)
        {
            test_property(range_begin + in_t::literal(sample));
        }
    }
};

TEST_F(rsqrt_property_test_sweep_t, sweep_low)
{
    sweep_range(in_t::literal(1));
}

TEST_F(rsqrt_property_test_sweep_t, sweep_low_reduced_range)
{
    // Maps to 0.5 in the reduced range. Sweeping across this crosses a power-of-two boundary, testing both the
    // transition of clz shifts and the minimax bounds.
    sweep_range(in_t::literal(in_value_t{1} << 31) - in_t::literal(sample_count / 2));
}

TEST_F(rsqrt_property_test_sweep_t, sweep_mid_reduced_range)
{
    // Maps to exactly 0.75 in the reduced range.
    sweep_range(in_t::literal(in_value_t{3} << 30) - in_t::literal(sample_count / 2));
}

TEST_F(rsqrt_property_test_sweep_t, sweep_high)
{
    sweep_range(in_t::literal(max<in_value_t>() - sample_count - 1));
}

// parameterized test for specific values
struct rsqrt_property_test_parameterized_t : rsqrt_property_test_t, WithParamInterface<in_t>
{
    in_t const x = GetParam();
};

TEST_P(rsqrt_property_test_parameterized_t, error_within_minimax_bounds)
{
    test_property(x);
};

in_t const vectors[] = {
    // large, odd clz (33)
    in_t::literal(max<int32_t>()),

    // large - 1, odd clz (33)
    in_t::literal(max<int32_t>() - 1),

    // power-of-2, even clz (34)
    in_t::literal(1u << 30),

    // below power-of-2, even clz (34)
    in_t::literal((1u << 30) - 1),

    // ~mouse magnitude, even clz (34)
    in_t::literal(2 * ((1u << 15) - 1) * ((1u << 15) - 1)),

    // small, even clz (62)
    in_t::literal(2),

    // smallest, odd clz (63)
    in_t::literal(1),
};
INSTANTIATE_TEST_SUITE_P(vectors, rsqrt_property_test_parameterized_t, ValuesIn(vectors));

} // namespace property_test

// --------------------------------------------------------------------------------------------------------------------
// value tests
// --------------------------------------------------------------------------------------------------------------------

namespace value_test {

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
static_assert(rsqrt((fixed_t<uint64_t, 61>(1) >> 1)) == fixed_t<uint64_t, 61>::literal(3260954456333195553ULL));

// pure integer input
// isqrt(100) = 0.1 ~= round(2^60/sqrt(100)) @Q60
// checks standard integer handling and large rescaling
static_assert(
    rsqrt<fixed_t<uint64_t, 60>>(fixed_t<uint64_t, 0>(100)) == fixed_t<uint64_t, 60>::literal(115292150460684698ULL));

// irrational non-power-of-2
// isqrt(3.0) ~= 0.577350269 ~= round(2^60/sqrt(3)) @Q60.
// checks rounding logic on typical but messy values
static_assert(rsqrt(fixed_t<uint64_t, 60>(3)) == fixed_t<uint64_t, 60>::literal(665639541039271462ULL));

// large upscale on small input
// isqrt(0.001*2^30) ~= 0.000965051 ~= round(2^30/sqrt(trunc(0.001*2^30)/2^30)) @Q30
static_assert(rsqrt(fixed_t<uint64_t, 30>::literal(static_cast<uint64_t>(0.001 * (1ULL << 30))))
    == fixed_t<uint64_t, 30>::literal(33954710857ULL));

// floor
// isqrt(2^63 - 1)
//     ~= 3.2927225399135962335354494746864465592611776064343944463441e-10
//     ~= round(2^60/sqrt(2^63 - 1)) @2^60
static_assert(
    rsqrt<fixed_t<uint64_t, 60>>(fixed_t<uint64_t, 0>(max<int64_t>())) == fixed_t<uint64_t, 60>::literal(379625062ULL));

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
// x ~= 0.60613493946432108055646607122248333065308120072574
//   ~= trunc(0.60613493946432108055646607122248333065308120072574*2^62)
//   ~= 2795304025607940678 @Q62
//
// The expected output is:
// 1.0/sqrt(0.60613493946432108055646607122248333065308120072574)
//  ~= 1.2844444916485788163793996641593715147224007703598051562247127687...
//  ~= round(2^62/sqrt(0.60613493946432108055646607122248333065308120072574))
//  ~= 5923454703581824736 @Q62
//
// The initial guess is:
// 10354071428996350879
//     - trunc(
//          2795304025607940678*(9674658516542619978 - trunc(2795304025607940678*3949951878332005776/2^62))/2^62
//       )
// ~= 10354071711462988194 - trunc(2795303719243043561*(9674659108971248202 - 2394203842659751666)/2^62)
// ~= 10354071711462988194 - trunc(2795303719243043561*7280455266311496536/2^62)
// ~= 10354071711462988194 - 4412937828461047064
// ~= 5941133883001941130 @q62
// ~= 11.2882780525955890593182984193987294929684139788150787353515625
//
// It starts with a guess of 1.2883 and must reach 1.2844 in NR steps alone.
static_assert(rsqrt(fixed_t<uint64_t, 62>::literal(2795304025607940678ULL))
    == fixed_t<uint64_t, 62>::literal(5923454703581824736ULL + 1));

} // namespace value_test
} // namespace rsqrt_test

} // namespace
} // namespace crv
