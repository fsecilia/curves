// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "rsqrt.hpp"
#include <crv/algorithm.hpp>
#include <crv/test/test.hpp>
#include <random>

namespace crv {
namespace {

static constexpr auto e_nr = uint128_t{14}; // error after 3 Newton-Raphson iterations, from sollya

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
        auto const difference = max(actual, expected) - std::min(actual, expected);
        auto const tolerance = wide_out_t::literal(2 * e_nr * uint128_t{x.value} * uint128_t{y.value});

        EXPECT_LT(difference, tolerance);
    }
};

TEST_F(rsqrt_property_test_t, fuzz)
{
    std::mt19937_64 rng{0xF012345678};
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

} // namespace property_test

} // namespace rsqrt_test

} // namespace
} // namespace crv
