// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "saturation.hpp"
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/io.hpp>
#include <crv/test/test.hpp>
#include <array>

namespace crv::spline::refinement_policies {
namespace {

using normalized_t = fixed_t<uint64_t, 32>;
using coeff_t = fixed_t<int64_t, 48>;

constexpr auto coeff_count = 4;
using monomial_t = std::array<coeff_t, coeff_count>;

using real_t = float_t;
using sut_t = saturation_t<real_t, normalized_t>;

template <typename fixed_t> constexpr auto convert(fixed_t src) noexcept -> fixed_t
{
    return src;
}

template <typename fixed_t> constexpr auto convert(std::integral auto literal) noexcept -> fixed_t
{
    return fixed_t::literal(literal);
}

template <typename fixed_t> constexpr auto convert(std::floating_point auto floating_point) noexcept -> fixed_t
{
    return to_fixed<fixed_t>(floating_point);
}

struct common_param_t
{
    bool expected;
    monomial_t monomial;

    common_param_t(bool expected, auto c0, auto c1, auto c2, auto c3) noexcept
        : expected{expected},
          monomial{convert<coeff_t>(c0), convert<coeff_t>(c1), convert<coeff_t>(c2), convert<coeff_t>(c3)}
    {}

    friend auto operator<<(std::ostream& out, common_param_t const& src) -> std::ostream&
    {
        out << "expected = " << src.expected;

        out << " .monomial = {" << src.monomial[0];
        for (auto coeff = 1; coeff < coeff_count; ++coeff) out << ", " << src.monomial[coeff];
        out << "}";

        return out;
    }
};

// --------------------------------------------------------------------------------------------------------------------
// interval tests
// --------------------------------------------------------------------------------------------------------------------

struct interval_param_t : common_param_t
{
    using common_param_t::common_param_t;

    friend auto operator<<(std::ostream& out, interval_param_t const& src) -> std::ostream&
    {
        return out << "{" << static_cast<common_param_t const&>(src) << "}";
    }
};

struct spline_refinement_policies_interval_test_t : TestWithParam<interval_param_t>
{
    bool expected = GetParam().expected;
    monomial_t const& monomial = GetParam().monomial;

    sut_t sut{};
};

TEST_P(spline_refinement_policies_interval_test_t, result)
{
    EXPECT_EQ(expected, sut(monomial));
}

interval_param_t const interval_params[] = {
    {false, 0, 0, 0, 0},
    {false, 1.0, 1.0, 1.0, 1.0},

    // degenerate quadratic, a = 0: peak in (0, 1) is safe
    // peak is at t = 0.5: p(0.5) = -400(0.25) + 400(0.5) = 100.
    {false, 0.0, -400.0, 400.0, 0.0},

    // full cubic, d > 0: two roots, but peak inside (0, 1) is safe
    // peak is at t = 0.5: p(0.5) = -50 + 150 + 3200 = 3300.
    {false, -400.0, 0.0, 300.0, 3200.0},

    // monotonic cubic, d < 0: saturates purely at the endpoint (t=1)
    // p(1) = 16000 + 16000 + 16000 + 0 = 48000 (> 32767)
    {true, 16000.0, 16000.0, 16000.0, 0.0},

    // degenerate quadratic, a = 0: peak in (0, 1) saturates
    // peak is at t = 0.5: p(0.5) = -32000(0.25) + 32000(0.5) + 32000 = 40000 (> 32767)
    {true, 0.0, -32000.0, 32000.0, 32000.0},

    // full cubic, d > 0: peak inside (0, 1) saturates
    // peak is at t = 0.5: p(0.5) = -4000(0.125) + 0 + 3000(0.5) + 32000 = 33000 (> 32767)
    {true, -4000.0, 0.0, 3000.0, 32000.0},

    // full cubic, d > 0: valley inside (0, 1), saturates negative
    // valley at t = 0.5: p(0.5) = 4000(0.125) + 0 - 3000(0.5) - 32000 = -33000 (< -32768)
    {true, 4000.0, 0.0, -3000.0, -32000.0},
};
INSTANTIATE_TEST_SUITE_P(interval_params, spline_refinement_policies_interval_test_t, ValuesIn(interval_params));

// --------------------------------------------------------------------------------------------------------------------
// point tests
// --------------------------------------------------------------------------------------------------------------------

struct point_param_t : common_param_t
{
    normalized_t point;

    point_param_t(bool expected, auto c0, auto c1, auto c2, auto c3, auto point) noexcept
        : common_param_t{expected, c0, c1, c2, c3}, point{convert<normalized_t>(point)}
    {}

    friend auto operator<<(std::ostream& out, point_param_t const& src) -> std::ostream&
    {
        return out << "{" << static_cast<common_param_t const&>(src) << ", .point = " << src.point << "}";
    }
};

struct spline_refinement_policies_point_test_t : TestWithParam<point_param_t>
{
    bool expected = GetParam().expected;
    monomial_t const& monomial = GetParam().monomial;
    normalized_t const& point = GetParam().point;

    sut_t sut{};
};

TEST_P(spline_refinement_policies_point_test_t, result)
{
    EXPECT_EQ(expected, sut(monomial, point));
}

point_param_t const point_params[] = {
    {false, 0, 0, 0, 0, 0},
    {false, 1.0, 1.0, 1.0, 1.0, 0.5},

    // saturates immediately on iteration 1
    // iter 1: 32000*0.5 + 32000 = 48000 (> 32767)
    {true, 32000.0, 32000.0, 0.0, 0.0, 0.5},

    // saturates the negative immediately on iteration 1
    {true, -32000.0, -32000.0, 0.0, 0.0, 0.5},

    // safe on iteration 1, but saturates on iteration 2
    // iter 1: 32000*0.5 + 0 = 16000
    // iter 2: 16000*0.5 + 32000 = 40000 (> 32767)
    {true, 32000.0, 0.0, 32000.0, 0.0, 0.5},

    // safe on iterations 1 & 2, saturates on iteration 3
    // iter 1: 32000*0.5 - 16000 = 0
    // iter 2: 0*0.5 + 16000 = 16000
    // iter 3: 16000*0.5 + 32000 = 40000 (> 32767)
    {true, 32000.0, -16000.0, 16000.0, 32000.0, 0.5},
};
INSTANTIATE_TEST_SUITE_P(point_params, spline_refinement_policies_point_test_t, ValuesIn(point_params));

} // namespace
} // namespace crv::spline::refinement_policies
