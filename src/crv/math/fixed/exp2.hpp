// SPDX-License-Identifier: MIT
/**
    \file
    \brief fixed point integer type

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/limits.hpp>

namespace crv {

/*!
    port of preprod exp2

    This is a port of the kernel c version we tested in preprod. Sollya says it should be is accurate up to 2^-61, but
    it uses a degree-12 polynomial and has a separate array for the coefficients of each exponent.

    We'll start with this version to help calibrate real implementations, but it is unlikely to stay around for
    production.
*/
class preprod_exp2_t
{
public:
    template <typename out_value_t, int_t out_frac_bits, typename in_value_t, int_t in_frac_bits>
    constexpr auto eval(fixed_t<in_value_t, in_frac_bits> const& input) const noexcept
        -> fixed_t<out_value_t, out_frac_bits>
    {
        uint64_t result, frac_part_norm;
        int      final_shift;
        int64_t  int_part;

        // Validate.
        static_assert(in_frac_bits < 64 || out_frac_bits < 64);

        // Reduce.

        // Save int part in Q64.0. This is part of the final shift.
        int_part = static_cast<int64_t>(input.value) >> in_frac_bits;
        if (int_part > 65) [[unlikely]] { return max<uint64_t>(); }
        if (int_part < -65) [[unlikely]] { return 0; }
        // int_part now fits into a standard int.

        // Normalize frac part into a Q0.64.
        // The input domain is now strictly [0, 1), and the output range is now [1, 2).
        if constexpr (in_frac_bits > 0) { frac_part_norm = static_cast<uint64_t>(input.value) << (64 - in_frac_bits); }
        else
        {
            frac_part_norm = 0;
        }

        // Approximate.

        // Apply Horner's method, but since the precision varies per coefficient, shift the difference between them
        // after each step.
        result = poly_coeffs[poly_degree];
        for (int i = poly_degree; i > 0; --i)
        {
            uint128_t product        = static_cast<uint128_t>(result) * frac_part_norm;
            int       relative_shift = poly_frac_bits[i] - poly_frac_bits[i - 1];
            int       total_shift    = relative_shift + 64;
            result                   = static_cast<uint64_t>(product >> total_shift) + poly_coeffs[i - 1];
        }

        // TODO: the last iteration should stay in 128 bits so we can shift into the final place.

        // Restore.

        // TODO: this is basically a runtime implementation of convert_value. Both should use the same core function.

        // At the end of the Horner's loop, the number of fractional bits in
        // result is the number of fractional bits of coefficient 0. Shift
        // the remaining int part, then shift into the final output precision.
        final_shift
            = static_cast<int>(out_frac_bits) - static_cast<int>(poly_frac_bits[0]) + static_cast<int>(int_part);
        using out_t = fixed_t<out_value_t, out_frac_bits>;
        if (final_shift > 0)
        {
            auto const shl = final_shift;
            if (shl >= 64) [[unlikely]] { return max<uint64_t>(); }
            return out_t{result << shl};
        }
        else if (final_shift < 0)
        {
            auto const shr = -final_shift;
            if (shr >= 64) [[unlikely]] { return 0; }
            return out_t{static_cast<out_value_t>(result >> shr) + ((result >> (shr - 1)) & 1ULL)};
        }
        else
        {
            return out_t{result};
        }
    }

private:
    // Output from tools/exp2.sollya.
    static constexpr auto     poly_degree   = 12;
    static constexpr uint64_t poly_coeffs[] = {
        4611686018427387904ULL, 6393154322601327706ULL, 8862793787191508053ULL, 8190960700631508079ULL,
        5677541315869497503ULL, 6296594800652510755ULL, 5819289539290670308ULL, 9219698356951991307ULL,
        6390833165122234360ULL, 7870198308678324976ULL, 8802550243955206649ULL, 8162192809866154575ULL,
        5762355121894017757ULL,
    };
    static constexpr int8_t poly_frac_bits[] = {
        62, 63, 65, 67, 69, 72, 75, 79, 82, 86, 90, 94, 97,
    };
};

} // namespace crv
