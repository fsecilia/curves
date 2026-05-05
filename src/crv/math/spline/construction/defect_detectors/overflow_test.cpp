// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "overflow.hpp"
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/io.hpp>
#include <crv/test/test.hpp>

#include <crv/math/spline/polynomial.hpp>

namespace crv::spline::defect_detectors {
namespace {

using normalized_t = fixed_t<uint64_t, 64>;
using coeff_t = fixed_t<int64_t, 48>;
using polynomial_t = cubic_polynomial_t<coeff_t>;

using real_t = float_t;
using sut_t = overflow_t<real_t, normalized_t, mac_t{}>;

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
    polynomial_t polynomial;

    common_param_t(bool expected, auto c0, auto c1, auto c2, auto c3) noexcept
        : expected{expected},
          polynomial{convert<coeff_t>(c0), convert<coeff_t>(c1), convert<coeff_t>(c2), convert<coeff_t>(c3)}
    {}

    friend auto operator<<(std::ostream& out, common_param_t const& src) -> std::ostream&
    {
        out << "expected = " << src.expected;

        out << " .polynomial = {" << src.polynomial[0];
        for (auto coeff = 1u; coeff < std::size(src.polynomial); ++coeff) out << ", " << src.polynomial[coeff];
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

struct spline_defect_detectors_interval_test_t : TestWithParam<interval_param_t>
{
    bool expected = GetParam().expected;
    polynomial_t const& polynomial = GetParam().polynomial;

    sut_t sut{};
};

TEST_P(spline_defect_detectors_interval_test_t, result)
{
    EXPECT_EQ(expected, sut(polynomial));
}

interval_param_t const interval_params[] = {
    //
    // trivial baselines
    //

    {false, 0.0, 0.0, 0.0, 0.0},
    {false, 1.0, 1.0, 1.0, 1.0},

    //
    // fast-path boundaries
    //

    // primal abs_sum = 32767 (at boundary, fast path fires)
    // tangent abs_sum = 16383 (fast path)
    {false, 0.0, 0.0, 16383.0, 16384.0},

    // primal abs_sum = 32768 (just past boundary, slow path entered)
    // degenerates to linear; mixed signs keep endpoint safe: -16384 + 16384 = 0
    // tangent abs_sum = 16384 (fast path)
    {false, 0.0, 0.0, -16384.0, 16384.0},

    // tangent abs_sum = 32767 (at boundary, fast path fires)
    // tangent = [24000, 8000, 767]. primal abs_sum = 12767 (fast path).
    {false, 8000.0, 4000.0, 767.0, 0.0},

    // tangent abs_sum = 32768 (just past boundary, slow path entered)
    // tangent = [24000, -8000, 768]. endpoint = 16768 (safe). peak at t = 1/6 ~ 101 (safe).
    // primal abs_sum = 12768 (fast path).
    {false, 8000.0, -4000.0, 768.0, 0.0},

    //
    // fast-path routing interactions
    //

    // primal fast filter passes (slow path skipped via &&), tangent fast filter fails,
    // tangent eval is safe. asymmetric: verifies tangent runs even when primal short-circuits.
    // primal abs sum: 8000 + 5000 + 4000 + 0 = 17000 (< 32767)
    // tangent abs sum: 24000 + 10000 + 4000 = 38000 (> 32767)
    // tangent q(t) = 24000t^2 - 10000t + 4000.
    //   endpoints: q(0) = 4000, q(1) = 18000. peak at t = 5/24 ~= 0.208 -> q ~= 2958.
    //   discriminant = 1e8 - 3*24000*4000 = -1.88e8 < 0; no synthetic cubic roots to check.
    {false, 8000.0, -5000.0, 4000.0, 0.0},

    // primal fast filter fails, tangent fast filter passes (slow path skipped via &&),
    // primal eval is safe. asymmetric: verifies tangent fast filter saves work correctly.
    // primal abs sum: 500 + 500 + 500 + 32000 = 33500 (> 32767)
    // tangent abs sum: 1500 + 1000 + 500 = 3000 (< 32767)
    // primal p(t) = 500t^3 + 500t^2 - 500t + 32000.
    //   endpoints: p(0) = 32000, p(1) = 32500. t_quadratic = -0.5 (rejected, strict left).
    //   discriminant = 1e6; roots at t = 1/3 (in) and t = -1 (out). p(1/3) ~= 31907 (safe).
    {false, 500.0, 500.0, -500.0, 32000.0},

    // fails both fast paths (sum of abs > 32767), but actual evaluation is completely safe
    // primal abs sum: 8000 + 10000 + 8000 + 10000 = 36000 (> 32767)
    // tangent abs sum: 24000 + 20000 + 8000 = 52000 (> 32767)
    // tangent q(t) = 24000t^2 - 20000t + 8000. endpoints: 8000, 12000. valley at t=0.416 is safe.
    // primal p(t) = 8000t^3 - 10000t^2 + 8000t - 10000. endpoints: -10000, -4000.
    {false, 8000.0, -10000.0, 8000.0, -10000.0},

    // both fast filters fail, both slow paths reject t_quadratic outside (0, 1) and find
    // the cubic root inside (0, 1) safe. exercises the discriminant > 0 branch under
    // a passing result; the existing both-fast-fail test has discriminant < 0 in both.
    // primal abs sum: 4000 + 6000 + 15000 + 10000 = 35000 (> 32767)
    // tangent abs sum: 12000 + 12000 + 15000 = 39000 (> 32767)
    // primal: t_quadratic = -6000/8000 = -0.75 (rejected, strict left).
    //   discriminant = 36e6 + 3*4000*15000 = 216e6; roots at t ~= 0.725 (in) and t ~= -1.725 (out).
    //   p(0.725) ~= 3804 (safe).
    // tangent synthetic [12000, 12000, -15000, 0]: t_quadratic = -0.5 (rejected, strict left).
    //   discriminant = 144e6 + 3*12000*15000 = 684e6; roots at t ~= 0.393 (in) and t ~= -1.060 (out).
    //   synthetic at t = 0.393 reaches ~= -3313 (safe).
    {false, 4000.0, 6000.0, -15000.0, 10000.0},

    //
    // degenerate degrees (linear & quadratic)
    //

    // pure linear, a = 0, b = 0: relies on endpoints
    // p(1) = 30000 + 0 = 30000
    {false, 0.0, 0.0, 30000.0, 0.0},

    // linear endpoint overflow: degenerates through c0 = 0, c1 = 0
    // endpoint = 32000 + 32000 = 64000 (> 32767)
    // tangent = [0, 0, 32000], abs_sum = 32000 (fast path)
    {true, 0.0, 0.0, 32000.0, 32000.0},

    // pure quadratic (a = 0), peak strict right of (0, 1), no endpoint overflow
    // t = -c/(2b) = 30000/20000 = 1.5 (rejected, strict right)
    // endpoints: p(0) = 0, p(1) = -20000 (safe)
    {false, 0.0, 10000.0, -30000.0, 0.0},

    // degenerate quadratic, a = 0: peak in (0, 1) overflows
    // peak is at t = 0.5: p(0.5) = -32000(0.25) + 32000(0.5) + 32000 = 40000 (> 32767)
    {true, 0.0, -32000.0, 32000.0, 32000.0},

    // pure quadratic, a = 0: peak at t = -c/(2b) = 16000/64000 = 0.25 overflows
    // p(0.25) = 32000(0.0625) - 16000(0.25) - 31000 = -33000 (< -32768)
    // p(0) = -31000 (in range), p(1) = -15000 (in range)
    {true, 0.0, 32000.0, -16000.0, -31000.0},

    // quadratic endpoint overflow (primal): degenerates through c0 = 0
    // partial sums: 16000, 32000, 48000 (> 32767 at third)
    // primal fires first; tangent would also overflow but is never reached
    {true, 0.0, 16000.0, 16000.0, 16000.0},

    // quadratic, peak left of (0, 1), slow path resolves safe
    // degenerates through c0 = 0. quadratic peak at t = -15000/10000 = -1.5 (rejected)
    // primal abs_sum = 33000 (> 32767). endpoint = 5000 + 15000 - 13000 = 7000 (safe).
    // tangent abs_sum = 25000 (fast path)
    {false, 0.0, 5000.0, 15000.0, -13000.0},

    //
    // tangent extrema & bounds
    //

    // tangent c0 bounds check overflows: 3 * c0 > 32767
    // c0 = 12000 -> 3 * c0 = 36000 (overflows).
    // primal is safe: peak at t = 0.5 is 12000(0.125) - 12000(0.25) = -1500. endpoints are 0.
    {true, 12000.0, -12000.0, 0.0, 0.0},

    // tangent c1 bounds check overflows negative: 2 * c1 < -32768
    // c1 = -17000 -> 2 * c1 = -34000 (overflows).
    // primal is safe: peak at t = 0.5 is -17000(0.25) + 17000(0.5) = 4250. endpoints are 0.
    {true, 0.0, -17000.0, 17000.0, 0.0},

    // tangent c0 bounds check overflows negative: 3 * c0 < -32768
    // c0 = -12000 -> 3 * c0 = -36000 (overflows).
    // primal is safe: peak at t = 0.5 is -12000(0.125) + 12000(0.25) = 1500.
    {true, -12000.0, 12000.0, 0.0, 0.0},

    // tangent c1 bounds check overflows positive: 2 * c1 > 32767
    // c1 = 17000 -> 2 * c1 = 34000 (overflows).
    // primal is safe: peak at t = 0.5 is 17000(0.25) - 17000(0.5) = -4250.
    {true, 0.0, 17000.0, -17000.0, 0.0},

    // tangent evaluates to overflow at endpoint, but coeffs and primal are safe
    // c0 = 10000, c1 = 10000. 3*c0 = 30000 (safe), 2*c1 = 20000 (safe).
    // tangent q(t) = 30000t^2 + 20000t. q(1) = 50000 (> 32767).
    // primal p(1) = 10000 + 10000 = 20000 (safe).
    {true, 10000.0, 10000.0, 0.0, 0.0},

    // tangent valley overflows negatively, but coeffs and primal are safe
    // c0 = 10666, c1 = -16000, c2 = -26000. 3*c0 = 31998 (safe), 2*c1 = -32000 (safe).
    // tangent q(t) = 31998t^2 - 32000t - 26000. valley at t ~= 0.5.
    // q(0.5) ~= 32000(0.25) - 32000(0.5) - 26000 = -34000 (< -32768).
    // primal p(1) = 10666 - 16000 - 26000 = -31334 (safe).
    {true, 10666.0, -16000.0, -26000.0, 0.0},

    // tangent intermediate (3a*t + 2b) overflows at t=1, but tangent value at t=1 is safe.
    // 3*c0 = 21000, 2*c1 = 16000; iter 1 at t=1: 21000 + 16000 = 37000 (> 32767, overflows).
    // tangent at t=1: 21000 + 16000 - 10000 = 27000 (would be safe).
    // primal is safe and short-circuits: abs sum = 7000 + 8000 + 10000 + 0 = 25000 (< 32767).
    // distinguishes the iter-1 intermediate check from the tangent-value check; without it,
    // the existing endpoint test does not require the iter-1 check to be present.
    {true, 7000.0, 8000.0, -10000.0, 0.0},

    //
    // cubic extrema (primal peaks, valleys, and roots)
    //

    // monotonic cubic, d < 0: overflows purely at the endpoint (t=1)
    // p(1) = 16000 + 16000 + 16000 + 0 = 48000 (> 32767)
    {true, 16000.0, 16000.0, 16000.0, 0.0},

    // primal endpoint early bailout: negative boundary blowout on first addition (c0 + c1)
    // partial sums: -20000, -40000 (< -32768, loop terminates immediately)
    {true, -20000.0, -20000.0, 0.0, 0.0},

    // full cubic, d > 0: peak in (0, 1) overflows
    // peak is at t = 0.5: p(0.5) = -4000(0.125) + 0 + 3000(0.5) + 32000 = 33000 (> 32767)
    {true, -4000.0, 0.0, 3000.0, 32000.0},

    // full cubic, d > 0: valley in (0, 1), overflows negative
    // valley at t = 0.5: p(0.5) = 4000(0.125) + 0 - 3000(0.5) - 32000 = -33000 (< -32768)
    {true, 4000.0, 0.0, -3000.0, -32000.0},

    // full cubic, d > 0: two roots in (0, 1), t0 is safe but t1 overflows
    // roots of derivative are at t=0.2 and t=0.8.
    // valley at t0 is safe, peak at t1 exceeds bounds.
    {true, 15000.0, -22500.0, 7200.0, -32000.0},

    // cubic, t_quadratic strict left and both cubic roots straddle (0, 1) outside the interval
    // t_quadratic = -b/(2a) = -1000/2000 = -0.5 (rejected, strict left)
    // discriminant = 1e6 - 3(1000)(-6000) = 1.9e7; sqrt ~= 4359
    //   t0 = (-1000 - 4359)/3000 ~= -1.79 (rejected, strict left)
    //   t1 = (-1000 + 4359)/3000 ~= 1.12  (rejected, strict right)
    // endpoints: p(0) = 0, p(1) = -4000 (safe)
    {false, 1000.0, 1000.0, -6000.0, 0.0},

    // intermediate quadratic overflows, but final cubic is safe
    // q(t) = -32000t^2 + 32000t + 30000. Peak at t=0.5 -> q(0.5) = 38000 (> 32767)
    // p(t) = q(t)t - 10000. Peak around t=0.5 is 38000(0.5) - 10000 = 9000
    {true, -32000.0, 32000.0, 30000.0, -10000.0},

    // cubic, intermediate quadratic q(t) = at^2 + bt + c overflows at t_quadratic = 0.25
    // q(0.25) = 32000(0.0625) - 16000(0.25) - 31000 = 2000 - 4000 - 31000 = -33000 (< -32768)
    // endpoints: p(0) = 0, p(1) = -15000; cubic root in (0,1) is at t ~= 0.759 with p ~= -18761
    {true, 32000.0, -16000.0, -31000.0, 0.0},

    // cubic with single root in (0, 1) at non-canonical t firing overflow
    // t_quadratic in (0, 1) is evaluated but safe: t_quadratic = -b/(2a) = 18000/60000 = 0.3
    //     q(0.3) = 30000(0.09) - 18000(0.3) + 0 = -2700; p(0.3) = -810 + d = -32710 (safe, just inside)
    // cubic roots of p'(t) = 90000t^2 - 36000t = 0 -> t = 0 (rejected) and t = 0.4
    //     p(0.4) = 30000(0.064) - 18000(0.16) + 0 + d = -960 + d = -32860 (overflows, just outside)
    // endpoints: p(0) = -31900 (safe), p(1) = -19900 (safe)
    {true, 30000.0, -18000.0, 0.0, -31900.0},

    // discriminant = 0: b^2 = 3ac. repeated root at t = 0.5 (not entered, branch requires > 0)
    // primal abs_sum = 45000 (> 32767). tangent abs_sum = 27000 (fast path).
    // t_quadratic = 6000/8000 = 0.75 (in interval, safe; worst intermediate = -31437)
    // endpoint = 4000 - 6000 + 3000 - 32000 = -31000 (safe)
    {false, 4000.0, -6000.0, 3000.0, -32000.0},

    // both cubic roots in (0, 1), both safe
    // roots of p'(t) at t = (15000 +/- 6000) / 30000 = {0.3, 0.7}
    // t_quadratic = 15000/20000 = 0.75 (in interval, safe)
    // worst Horner intermediate across all three: -15000 at iter 1 of t = 0.3
    // primal abs_sum = 33300 (> 32767). endpoint = 3300 (safe).
    // tangent = [30000, -30000, 6300], abs_sum = 66300. endpoint = 6300 (safe).
    //  tangent peak at t = 0.5: 7500 - 15000 + 6300 = -1200 (safe)
    {false, 10000.0, -15000.0, 6300.0, 2000.0},
};
INSTANTIATE_TEST_SUITE_P(interval_params, spline_defect_detectors_interval_test_t, ValuesIn(interval_params));

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

struct spline_defect_detectors_point_test_t : TestWithParam<point_param_t>
{
    bool expected = GetParam().expected;
    polynomial_t const& polynomial = GetParam().polynomial;
    normalized_t const& point = GetParam().point;

    sut_t sut{};
};

TEST_P(spline_defect_detectors_point_test_t, result)
{
    EXPECT_EQ(expected, sut(polynomial, point));
}

point_param_t const point_params[] = {
    //
    // trivial baselines
    //

    {false, 0.0, 0.0, 0.0, 0.0, 0.0},
    {false, 1.0, 1.0, 1.0, 1.0, 0.5},

    //
    // intermediate routing
    //

    // overflows immediately on iteration 1
    // iter 1: 32000*0.5 + 32000 = 48000 (> 32767)
    {true, 32000.0, 32000.0, 0.0, 0.0, 0.5},

    // overflows the negative immediately on iteration 1
    {true, -32000.0, -32000.0, 0.0, 0.0, 0.5},

    // safe on iteration 1, but overflows on iteration 2
    // iter 1: 32000*0.5 + 0 = 16000
    // iter 2: 16000*0.5 + 32000 = 40000 (> 32767)
    {true, 32000.0, 0.0, 32000.0, 0.0, 0.5},

    // safe on iterations 1 & 2, overflows on iteration 3
    // iter 1: 32000*0.5 - 16000 = 0
    // iter 2: 0*0.5 + 16000 = 16000
    // iter 3: 16000*0.5 + 32000 = 40000 (> 32767)
    {true, 32000.0, -16000.0, 16000.0, 32000.0, 0.5},
};
INSTANTIATE_TEST_SUITE_P(point_params, spline_defect_detectors_point_test_t, ValuesIn(point_params));

} // namespace
} // namespace crv::spline::defect_detectors
