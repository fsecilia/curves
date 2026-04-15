// SPDX-License-Identifier: MIT

/// \file
/// \brief fixed point fast rsqrt
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/fma.hpp>
#include <crv/math/limits.hpp>
#include <crv/math/shifter.hpp>
#include <array>
#include <bit>

namespace crv {

namespace rsqrt_initial_guesses {

struct quadratic_minimax_t
{
    using in_t    = fixed_t<uint64_t, 64>;
    using coeff_t = fixed_t<uint64_t, 62>;
    using out_t   = coeff_t;

    // Quadratic approximation coefficients.
    //
    // The constants are in Q2.62, so they're scaled by 2^62 and rounded. See
    // tools/sollya/gen_rsqrt_initial_guess.sollya for more information about how these values are generated.
    static constexpr auto coeff_count = 3;
    static constexpr auto coeffs      = std::array<coeff_t, coeff_count>{
        coeff_t::literal(10354071711462988194ULL),
        coeff_t::literal(9674659108971248202ULL),
        coeff_t::literal(3949952137299739940ULL),
    };

    // \pre in must be in [0.5, 1); upper bound is automatic since 1.0 is unrepresentable in unsigned Q0.64
    static constexpr auto operator()(in_t in) noexcept -> out_t
    {
        assert(in.value >= (in_t::value_t{1} << (in_t::frac_bits - 1)));

        // apply horner's method: C0 + -C1*x + C2*x^2 = C0 - x*(C1 - x*C2)
        auto const inner = coeff_t::convert(multiply(in, coeffs[2]));
        auto const outer = coeff_t::convert(multiply(in, coeffs[1] - inner));
        return coeffs[0] - outer;
    }
};

} // namespace rsqrt_initial_guesses

/// reciprocal sqrt
///
/// This function solves `y = 1/sqrt(x)` using Newton-Raphson. We define a function, `f(y)`, with the same roots as y,
/// start with an initial guess near the solution, then iterate using the recurrence relation:
///
///     `y[n + 1] = y[n] - f(y[n])/f'(y[n])`
///
/// Each step finds the line tangent to `f(y[n])`, finds the horizontal intercept of that tangent, then repeats with
/// `y[n + 1]` set to that intercept. With a good initial guess for `y[0]`, this converges quadratically to the root of
/// `f(y)`.
///
/// In the case of `y = 1/sqrt(x)`, we choose `f(y) = y^-2 - x`:
///
///     ```
///     y = 1/sqrt(x)   // given
///     y^2 = 1/x       // square both sides
///     xy^2 = 1        // multiply both sides by x
///     x = y^-2        // divide both sides by y^2
///     0 = y^-2 - x    // find root
///     ```
///
/// There are other choices, but this has an important property. Given `f'(y) = -2y^-3`:
///
///     ```
///     y + f(y)/f'(y) = y - (y^-2 - x)/(-2y^-3)   // given
///                    = y + y^3(y^-2 - x)/2       // multiply right by -y^3/-y^3
///                    = y + y(1 - xy^2)/2         // distribute y^2
///                    = y(1 + (1 - xy^2)/2)       // factor out common y
///                    = y(3 - xy^2)/2             // combine constants
///     ```
///
/// This form allows calculating isqrt using only multiplication, subtraction, and a shift.
///
/// The initial guess is found using a quadratic approximation of `1/sqrt(x)` using Horner's method:
///
///     `C0 + -C1*x + C2*x^2 = C0 - x*(C1 - x*C2)`.
///
/// Using a quadratic approximation balances Horner iterations against Newton-Raphson iterations. Each NR iteration uses
/// 3 multiplies. Horner iterations use 1. For the same precision, a -log2/2 approximation requires 6 iterations. Linear
/// requires 4. Quadratic requires 3. Cubic also requires 3, so we use quadratic.
template <is_fixed out_t, is_fixed in_t = out_t, int_t nr_iteration_count = 3,
          typename initial_guess_t = rsqrt_initial_guesses::quadratic_minimax_t>
struct rsqrt_t
{
    static constexpr auto frac_bits        = in_t::frac_bits;
    static constexpr auto output_frac_bits = out_t::frac_bits;

    static constexpr auto x_norm_frac_bits = 64;
    static constexpr auto y_frac_bits      = 62;
    static constexpr auto three_q62        = 3ULL << 62;
    static constexpr auto sqrt2_q62        = 0x5A827999FCEF3242ULL;

    [[no_unique_address]] initial_guess_t initial_guess{};

    template <typename shifter_t = shifter_t<>>
    constexpr auto operator()(in_t in, shifter_t shifter = shifter_t{}) noexcept -> out_t
    {
        auto const x = in.value;

        if (x == 0) [[unlikely]] { return fixed_t<uint64_t, output_frac_bits>::literal(max<uint64_t>()); }

        // Normalize x to Q0.64 [0.5, 1.0).
        auto const x_lz            = std::countl_zero(x);
        auto const x_norm          = static_cast<uint64_t>(x << x_lz);
        auto const x_norm_exponent = x_lz + frac_bits;

        // Newton-Raphson.
        auto y = initial_guess(fixed_t<uint64_t, 64>::literal(x_norm)).value;
        for (int_t i = 0; i < nr_iteration_count; ++i)
        {
            auto const yy     = static_cast<uint64_t>((uint128_t{y} * y) >> y_frac_bits);
            auto const factor = static_cast<uint64_t>((uint128_t{x_norm} * yy) >> x_norm_frac_bits);
            y                 = static_cast<uint64_t>((uint128_t{y} * (three_q62 - factor)) >> (y_frac_bits + 1));
        }

        // Denormalize.
        if (x_norm_exponent & 1) y = static_cast<uint64_t>((static_cast<uint128_t>(y) * sqrt2_q62) >> y_frac_bits);

        auto const y_denorm_frac_bits = y_frac_bits + (x_norm_frac_bits >> 1) - (x_norm_exponent >> 1);

        // Handle invalid scales.
        if (y_denorm_frac_bits >= 128 || output_frac_bits >= 128) [[unlikely]]
        {
            // zero values and right shifts return 0
            if (!y || output_frac_bits < y_denorm_frac_bits) return out_t{0};

            // nonzero value left shifted 128 saturates
            return max<out_t>();
        }

        auto const result = shifter.shift(uint128_t{y}, output_frac_bits - y_denorm_frac_bits);
        if (result > max<out_t>().value) [[unlikely]] { return max<out_t>(); }

        return out_t::literal(static_cast<uint64_t>(result));
    }
};

} // namespace crv
