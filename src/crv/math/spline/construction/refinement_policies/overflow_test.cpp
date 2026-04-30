// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "overflow.hpp"
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
    {false, 0.0, 0.0, 0.0, 0.0},
    {false, 1.0, 1.0, 1.0, 1.0},

    // pure linear, a = 0, b = 0: relies on endpoints
    // p(1) = 30000 + 0 = 30000
    {false, 0.0, 0.0, 30000.0, 0.0},

    // degenerate quadratic, a = 0: peak in (0, 1) is safe
    // peak is at t = 0.5: p(0.5) = -400(0.25) + 400(0.5) = 100.
    {false, 0.0, -400.0, 400.0, 0.0},

    // pure quadratic (a = 0), peak strict right of (0, 1), no endpoint saturation
    // t = -c/(2b) = 30000/20000 = 1.5 (rejected, strict right)
    // endpoints: p(0) = 0, p(1) = -20000 (safe)
    {false, 0.0, 10000.0, -30000.0, 0.0},

    // pure quadratic (a = 0), peak strict left of (0, 1), no endpoint saturation
    // t = -c/(2b) = -3000/2000 = -1.5 (rejected, strict left)
    // endpoints: p(0) = 0, p(1) = 4000 (safe)
    {false, 0.0, 1000.0, 3000.0, 0.0},

    // degenerate quadratic, a = 0: peak in (0, 1) saturates
    // peak is at t = 0.5: p(0.5) = -32000(0.25) + 32000(0.5) + 32000 = 40000 (> 32767)
    {true, 0.0, -32000.0, 32000.0, 32000.0},

    // pure quadratic, a = 0: peak at t = -c/(2b) = 16000/64000 = 0.25 saturates
    // p(0.25) = 32000(0.0625) - 16000(0.25) - 31000 = -33000 (< -32768)
    // p(0) = -31000 (in range), p(1) = -15000 (in range)
    {true, 0.0, 32000.0, -16000.0, -31000.0},

    // cubic, t_quadratic strict left and both cubic roots straddle (0, 1) outside the interval
    // t_quadratic = -b/(2a) = -1000/2000 = -0.5 (rejected, strict left)
    // discriminant = 1e6 - 3(1000)(-6000) = 1.9e7; sqrt ~= 4359
    //   t0 = (-1000 - 4359)/3000 ~= -1.79 (rejected, strict left)
    //   t1 = (-1000 + 4359)/3000 ~= 1.12  (rejected, strict right)
    // endpoints: p(0) = 0, p(1) = -4000 (safe)
    {false, 1000.0, 1000.0, -6000.0, 0.0},

    // full cubic, d > 0: two roots, but peak inside (0, 1) is safe
    // peak is at t = 0.5: p(0.5) = -50 + 150 + 3200 = 32100.
    {false, -400.0, 0.0, 300.0, 32000.0},

    // monotonic cubic, d < 0: saturates purely at the endpoint (t=1)
    // p(1) = 16000 + 16000 + 16000 + 0 = 48000 (> 32767)
    {true, 16000.0, 16000.0, 16000.0, 0.0},

    // full cubic, d > 0: peak in (0, 1) saturates
    // peak is at t = 0.5: p(0.5) = -4000(0.125) + 0 + 3000(0.5) + 32000 = 33000 (> 32767)
    {true, -4000.0, 0.0, 3000.0, 32000.0},

    // full cubic, d > 0: valley in (0, 1), saturates negative
    // valley at t = 0.5: p(0.5) = 4000(0.125) + 0 - 3000(0.5) - 32000 = -33000 (< -32768)
    {true, 4000.0, 0.0, -3000.0, -32000.0},

    // full cubic, d > 0: two roots in (0, 1), t0 is safe but t1 saturates
    // roots of derivative are at t=0.2 and t=0.8.
    // valley at t0 is safe, peak at t1 exceeds bounds.
    {true, 15000.0, -22500.0, 7200.0, -32000.0},

    // intermediate quadratic saturates, but final cubic is safe
    // q(t) = -32000t^2 + 32000t + 30000. Peak at t=0.5 -> q(0.5) = 38000 (> 32767)
    // p(t) = q(t)t - 10000. Peak around t=0.5 is 38000(0.5) - 10000 = 9000
    {true, -32000.0, 32000.0, 30000.0, -10000.0},

    // cubic, intermediate quadratic q(t) = at^2 + bt + c saturates at t_quadratic = 0.25
    // q(0.25) = 32000(0.0625) - 16000(0.25) - 31000 = 2000 - 4000 - 31000 = -33000 (< -32768)
    // endpoints: p(0) = 0, p(1) = -15000; cubic root in (0,1) is at t ~= 0.759 with p ~= -18761
    {true, 32000.0, -16000.0, -31000.0, 0.0},

    // cubic with single root in (0, 1) at non-canonical t firing saturation
    // t_quadratic in (0, 1) is evaluated but safe: t_quadratic = -b/(2a) = 18000/60000 = 0.3
    //     q(0.3) = 30000(0.09) - 18000(0.3) + 0 = -2700; p(0.3) = -810 + d = -32710 (safe, just inside)
    // cubic roots of p'(t) = 90000t^2 - 36000t = 0 -> t = 0 (rejected) and t = 0.4
    //     p(0.4) = 30000(0.064) - 18000(0.16) + 0 + d = -960 + d = -32860 (saturates, just outside)
    // endpoints: p(0) = -31900 (safe), p(1) = -19900 (safe)
    {true, 30000.0, -18000.0, 0.0, -31900.0},
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
    {false, 0.0, 0.0, 0.0, 0.0, 0.0},
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
