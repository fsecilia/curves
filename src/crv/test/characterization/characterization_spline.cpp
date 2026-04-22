// SPDX-License-Identifier: MIT

/// \file
/// \brief characterizes various bit distributions in a packed spline segment
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/math/abs.hpp>
#include <crv/math/integer.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/math/mesh/quantizer.hpp>
#include <crv/math/spline/fixed/cubic_monomial.hpp>
#include <crv/math/spline/float/io.hpp>
#include <crv/math/spline/float/segment.hpp>
#include <crv/ranges.hpp>
#include <cstdlib>
#include <iostream>
#include <random>

namespace crv {
namespace {

using real_t = float_t;
using jet_t = jet_t<real_t>;
template <int frac_bits> using quantizer_t = quantizers::fixed_point_t<real_t, frac_bits>;

using monomial_segment_t = spline::floating_point::segment_t<real_t>;
struct hermite_segment_t
{
    static constexpr auto knot_count = 2;
    std::array<jet_t, knot_count> knots;

    friend auto operator<<(std::ostream& out, hermite_segment_t const& src) -> std::ostream&
    {
        return out << "{.knots = {" << src.knots[0] << ", " << src.knots[1] << "}}";
    }
};

auto quantize(real_t value, int_t frac_bits) noexcept -> real_t
{
    using std::ldexp;
    using std::rint;

    // scale up by directly manipulating the exponent
    auto const scaled = ldexp(value, int_cast<int>(frac_bits));

    // round nearest even and scale back down
    return ldexp(rint(scaled), int_cast<int>(-frac_bits));
}

struct quantization_t
{
    using precisions_t = std::array<int_t, monomial_segment_t::coeff_count>;
    precisions_t precisions;

    constexpr auto quantize(monomial_segment_t segment) const noexcept -> monomial_segment_t
    {
        for (auto coeff = 0; coeff < monomial_segment_t::coeff_count; ++coeff)
        {
            segment.coeffs[coeff] = crv::quantize(segment.coeffs[coeff], precisions[coeff]);
        }
        return segment;
    }

    friend auto operator<<(std::ostream& out, quantization_t const& src) -> std::ostream&
    {
        out << "{" << src.precisions[0];
        for (auto coeff = 1u; coeff < std::size(src.precisions); ++coeff) out << ", " << src.precisions[coeff];
        out << "}";

        return out;
    }
};

auto to_hermite(monomial_segment_t const& src) noexcept -> hermite_segment_t
{
    return {
        jet_t{src.coeffs[3], src.coeffs[2]},
        jet_t{src.coeffs[0] + src.coeffs[1] + src.coeffs[2] + src.coeffs[3],
            3 * src.coeffs[0] + 2 * src.coeffs[1] + src.coeffs[2]},
    };
}

auto to_monomial(hermite_segment_t const& src) noexcept -> monomial_segment_t
{
    return {
        .coeffs = {
            2.0 * (primal(src.knots[0]) - primal(src.knots[1])) + derivative(src.knots[0]) + derivative(src.knots[1]),
            3.0 * (primal(src.knots[1]) - primal(src.knots[0])) - 2.0 * derivative(src.knots[0]) - derivative(src.knots[1]),
            derivative(src.knots[0]),
            primal(src.knots[0]),
        },
    };
}

auto is_monotonic_increasing(hermite_segment_t const& hermite_segment) noexcept -> bool
{
    using std::swap;

    auto const dp = primal(hermite_segment.knots[1]) - primal(hermite_segment.knots[0]);
    if (dp <= 0.0) return 0.0 == dp;

    auto const a = derivative(hermite_segment.knots[0]) / dp;
    auto const b = derivative(hermite_segment.knots[1]) / dp;

    // reject opposing signs; since dp is positive, a or b < 0 means an m was negative
    if (a < 0.0 || b < 0.0) return false;

    // Fritsch-Carlson inner square
    if (a + b <= 3.0) return true;

    // expanded symmetric boundary
    return (a * a + a * b + b * b - 6.0 * (a + b) + 9.0) <= 0.0;
}

auto generate_fritsch_carlson_hermite_segment(auto& rng) noexcept -> hermite_segment_t
{
    using std::swap;

    // p1 >= p0
    auto unit_distribution = std::uniform_real_distribution<real_t>{};
    auto p0 = unit_distribution(rng);
    auto p1 = unit_distribution(rng);
    auto const decreasing = p1 < p0;
    if (decreasing) swap(p0, p1);

    // m0 < 3dp, m1 < 3dp
    auto const dp = p1 - p0;
    auto const threshold = 3.0 * dp;
    auto m_distribution = std::uniform_real_distribution<real_t>{0.0, threshold};
    auto const m0 = m_distribution(rng);
    auto const m1 = m_distribution(rng);

    return {jet_t{p0, m0}, jet_t{p1, m1}};
}

auto generate_monotonic_hermite_segment(auto& rng) noexcept -> hermite_segment_t
{
    using std::swap;

    // p1 >= p0
    auto primal_distribution = std::uniform_real_distribution<real_t>{0.0, 10.0};
    auto p0 = primal_distribution(rng);
    auto p1 = primal_distribution(rng);
    auto const decreasing = p1 < p0;
    if (decreasing) swap(p0, p1);

    auto const dp = p1 - p0;
    if (0.0 == dp) return {jet_t{p0, 0.0}, jet_t{p1, 0.0}};

    // rejection sampling has 56% chance of succeeding
    auto alpha_beta_dist = std::uniform_real_distribution<real_t>{0.0, 4.0};
    auto a = 0.0;
    auto b = 0.0;
    while (true)
    {
        a = alpha_beta_dist(rng);
        b = alpha_beta_dist(rng);

        // Fritsch-Carlson inner square
        if (a + b <= 3.0) break;

        // expanded symmetric boundary
        if ((a * a + a * b + b * b - 6.0 * (a + b) + 9.0) <= 0.0) break;
    }

    auto const result = hermite_segment_t{jet_t{p0, a * dp}, jet_t{p1, b * dp}};
    assert(is_monotonic_increasing(result));
    return result;
}

using coeff_t = int64_t;
struct fixed_monomial_t
{
    static constexpr auto coeff_count = 4;

    std::array<coeff_t, coeff_count> coeffs;
    constexpr auto operator()(uint64_t t) const -> uint64_t
    {
        using wide_t = widened_t<coeff_t>;

        shifter_t<rounding_modes::shr::fast::nearest_up_t> shifter;

        auto result = coeffs[0];
        for (auto coeff = 1; coeff < coeff_count; ++coeff)
        {
            auto const product = wide_t{result} * t;
            auto const sum = shifter.shr(product, 32) + coeffs[coeff];
            result = int_cast<coeff_t>(sum);
        }

        return result;
    }
};

auto analyze_segment(monomial_segment_t const& unquantized, monomial_segment_t const& quantized) -> bool
{
    if (!is_monotonic_increasing(to_hermite(quantized)))
    {
        std::cout << "not monotonic!" << std::endl;
        return false;
    }

    using std::rint;

    auto const scale = 1ull << 32;
    auto const fixed_monomial = fixed_monomial_t{.coeffs = {
                                                     static_cast<coeff_t>(rint(quantized.coeffs[0] * scale)),
                                                     static_cast<coeff_t>(rint(quantized.coeffs[1] * scale)),
                                                     static_cast<coeff_t>(rint(quantized.coeffs[2] * scale)),
                                                     static_cast<coeff_t>(rint(quantized.coeffs[3] * scale)),
                                                 }};

    auto const num_iterations = 100'000'000;
    auto const step_size = 1.0 / num_iterations;
    auto max_error = 0.0;
    for (auto iteration = 0; iteration < num_iterations; ++iteration)
    {
        auto const t = quantize(iteration * step_size, 32);
        auto const ideal = unquantized.evaluate(t);
        auto const t_fixed = static_cast<uint64_t>(t * scale);
        auto const actual_fixed = fixed_monomial(t_fixed);
        auto const actual = static_cast<real_t>(actual_fixed) / scale;
        auto const error = abs(actual - ideal);
        max_error = std::max(error, max_error);
    }
    std::cout << "max_error: " << max_error << std::endl;

    return true;
}

auto analyze_hermite_segment_with_varying_precisions(
    hermite_segment_t const& hermite_segment, compatible_range<quantization_t> auto const& quantizations) -> bool
{
    auto const monomial_segment = to_monomial(hermite_segment);
    std::cout << "monomial = " << monomial_segment << std::endl;
    for (auto const& quantization : quantizations)
    {
        auto const quantized_monomial_segment = quantization.quantize(monomial_segment);
        std::cout << "quantized monomial = " << quantized_monomial_segment << ", quantization = " << quantization
                  << std::endl;
        if (!analyze_segment(monomial_segment, quantized_monomial_segment)) return false;
    }
    return true;
}

auto analyze_hermite_segment_with_variations(
    hermite_segment_t const& hermite_segment, compatible_range<quantization_t> auto const& quantizations) -> bool
{
    std::cout << "analying segment = " << hermite_segment << std::endl;
    if (!analyze_hermite_segment_with_varying_precisions(hermite_segment, quantizations)) return false;

#if 0
    auto limit_taper = hermite_segment;
    limit_taper.knots[1].df = 0.0;
    std::cout << "limit taper variation = " << limit_taper << std::endl;
    if (!analyze_hermite_segment_with_varying_precisions(limit_taper, quantizations)) return false;

    auto hard_kick = hermite_segment;
    hard_kick.knots[0].df = 0.0;
    hard_kick.knots[1].df = 4 * (primal(hard_kick.knots[1]) - primal(hard_kick.knots[0]));
    std::cout << "hard kick variation = " << hard_kick << std::endl;
    if (!analyze_hermite_segment_with_varying_precisions(hard_kick, quantizations)) return false;
#endif

    return true;
}

auto main(int, char*[]) -> int
{
    auto rng = std::mt19937_64{0xF012345678};

    constexpr quantization_t quantizations[] = {
        {32, 32, 32, 32},
#if 0
#if 1
        // uniform
        {45, 45, 45, 45},
        {32, 32, 32, 32},
        {16, 16, 16, 16},
        {8, 8, 8, 8},
#endif

        // 1 flush variations
        {28, 50, 50, 50}, // funnel
        {34, 50, 44, 50}, // Fritsch-Carlson bias, maximized b

        // 2 flush variation
        {36, 46, 46, 50},
#endif
    };

#if 1
    constexpr hermite_segment_t hermite_segments[] = {
        // 1. Pure Linear:
        // Forces a=0 and b=0. Tests if the pipeline correctly handles
        // pure uniform velocities without losing precision on the c coefficient.
        {{{{0.0, 2.0}, {100.0, 2.0}}}},

        // 2. The Infinite Taper (Fractional Starvation):
        // a=0, b=0, c=0. Tests pure DC offset. If your RNE shifter or
        // flush math is wrong, this horizontal line will jitter.
        {{{{10.0, 0.0}, {10.0, 0.0}}}},

        // 3. The Heaviside (Max Integer Overflow):
        // Massive dy, zero tangents. This forces the polynomial to bridge
        // a massive gap purely using the 'b' coefficient, maximizing integer growth.
        {{{{0.0, 0.0}, {255.0, 0.0}}}},

        // 4. The Ellipse Apex (Symmetric Tangent Stress):
        // a=3, b=3. The absolute mathematical limit of symmetric monotonicity.
        // The slope perfectly kisses zero at exactly t=0.5.
        // Generates massive, opposing 'a' and 'b' coefficients that cancel out.
        {{{{0.0, 300.0}, {100.0, 300.0}}}},

        // 5. The Asymmetric Cusp (Right Anchor):
        // a=3, b=0. Hits the extreme right x-intercept of the monotonicity ellipse.
        // The slope perfectly kisses zero at exactly t=1.0.
        {{{{0.0, 300.0}, {100.0, 0.0}}}},

        // 6. The Asymmetric Cusp (Left Anchor):
        // a=0, b=3. Hits the extreme left y-intercept of the ellipse.
        // The slope starts at zero at t=0.0 and accelerates.
        {{{{0.0, 0.0}, {100.0, 300.0}}}},

        // 7. The Micro-Step (Fractional Underflow):
        // Microscopic dy, near-zero slopes. Tests if the accumulator
        // drops critical bits when the math happens entirely in the deep fractional radices.
        {{{{0.5, 1e-5}, {0.5 + 1e-5, 1e-5}}}},

        // 8. Large Origin Micro-Step (Radix Starvation):
        // Same as above, but starting at a massive physical count offset.
        // Tests if the large integer 'd' coefficient steals too much radix space,
        // destroying the micro-fractional additions of the curve.
        {{{{250.0, 1e-5}, {250.0 + 1e-5, 1e-5}}}},
    };

    std::cout << "...-=( specific segments )=-..." << std::endl;
    for (auto const specific_segment : hermite_segments)
    {
        if (!analyze_hermite_segment_with_variations(specific_segment, quantizations)) return EXIT_FAILURE;
    }
#endif

    std::cout << "...-=( random segments )=-..." << std::endl;
    auto const num_random_segments = 10;
    for (auto random_segment = 0; random_segment < num_random_segments; ++random_segment)
    {
        if (!analyze_hermite_segment_with_variations(generate_monotonic_hermite_segment(rng), quantizations))
        {
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

} // namespace
} // namespace crv

auto main(int arg_count, char* args[]) -> int
{
    return crv::main(arg_count, args);
}
