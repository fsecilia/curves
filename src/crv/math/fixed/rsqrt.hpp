// SPDX-License-Identifier: MIT

/// \file
/// \brief fixed point fast rsqrt
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/limits.hpp>
#include <crv/math/rounding_mode.hpp>
#include <bit>

namespace crv {

using u64                      = uint64_t;
using u128                     = uint128_t;
constexpr auto U64_MAX         = max<u64>();
constexpr auto CURVES_U128_MAX = max<u128>();

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
constexpr auto rsqrt(u64 x, unsigned int frac_bits, unsigned int output_frac_bits) noexcept -> u64
{
    // Quadratic approximation coefficients.
    //
    // The constants are in Q2.62, so they're scaled by 2^62 and rounded. See
    // src/curves/tools/isqrt_initial_guess.sollya for more information about how these values are generated.
    u64 c0_q62 = 10354071711462988194ULL;
    u64 c1_q62 = 9674659108971248202ULL;
    u64 c2_q62 = 3949952137299739940ULL;

    unsigned int x_norm_frac_bits = 64;
    unsigned int y_frac_bits      = 62;
    u64          three_q62        = 3ULL << 62;
    u64          sqrt2_q62        = 0x5A827999FCEF3242ULL;

    unsigned int x_lz, x_norm_exponent, y_denorm_frac_bits;
    u64          c1, c2, x_norm, y, yy, factor;

    if (x == 0) [[unlikely]] { return U64_MAX; }

    // Normalize x to Q0.64 [0.5, 1.0).
    x_lz            = std::countl_zero(x);
    x_norm          = x << x_lz;
    x_norm_exponent = x_lz + frac_bits;

    // Approximate 1/sqrt for initial guess using Horner's method.
    c2 = static_cast<u64>((static_cast<u128>(x_norm) * c2_q62) >> x_norm_frac_bits);
    c1 = static_cast<u64>((static_cast<u128>(x_norm) * (c1_q62 - c2)) >> x_norm_frac_bits);
    y  = c0_q62 - c1;

    // Newton-Raphson.
    for (int i = 0; i < 3; ++i)
    {
        yy     = static_cast<u64>((static_cast<u128>(y) * y) >> y_frac_bits);
        factor = static_cast<u64>((static_cast<u128>(x_norm) * yy) >> x_norm_frac_bits);
        y      = static_cast<u64>((static_cast<u128>(y) * (three_q62 - factor)) >> (y_frac_bits + 1));
    }

    // Denormalize.
    if (x_norm_exponent & 1) y = static_cast<u64>((static_cast<u128>(y) * sqrt2_q62) >> y_frac_bits);
    y_denorm_frac_bits = y_frac_bits + (x_norm_frac_bits >> 1) - (x_norm_exponent >> 1);

    u128 y_128 = static_cast<u128>(y);

    // Handle invalid scales.
    if (y_denorm_frac_bits >= 128 || output_frac_bits >= 128) [[unlikely]]
    {
        // Zero values and right shifts return 0.
        if (y_128 == 0 || output_frac_bits < y_denorm_frac_bits) return 0;

        return U64_MAX;
    }

    u128 result;
    if (output_frac_bits < y_denorm_frac_bits)
    {
        unsigned int shift = y_denorm_frac_bits - output_frac_bits;

        u128 half      = static_cast<u128>(1) << (shift - 1);
        u128 frac_mask = (static_cast<u128>(1) << shift) - 1;

        u128 int_part  = y_128 >> shift;
        u128 frac_part = y_128 & frac_mask;

        u128 is_odd = int_part & 1;

        u128 bias  = half - 1 + is_odd;
        u128 carry = (frac_part + bias) >> shift;

        result = int_part + carry;
    }
    else
    {
        unsigned int shift = output_frac_bits - y_denorm_frac_bits;

        // Find the maximum value that doesn't overflow.
        u128 max_safe_val = CURVES_U128_MAX >> shift;
        if (y_128 > max_safe_val) [[unlikely]]
            return U64_MAX;

        // The value is safe to shift.
        result = y_128 << shift;
    }

    if (result > static_cast<u128>(U64_MAX)) [[unlikely]] { return U64_MAX; }

    return static_cast<u64>(result);
}

} // namespace crv
