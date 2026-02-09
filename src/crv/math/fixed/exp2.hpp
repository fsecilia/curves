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
#include <limits>

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

        // TODO: this is basically a runtime implementation of fixed_t::convert_value. Both should use the same core
        // function.

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

class exp2_q32_t
{
public:
    template <unsigned_integral out_value_t, int_t out_frac_bits, integral in_value_t, int_t in_frac_bits>
    constexpr auto eval(fixed_t<in_value_t, in_frac_bits> const& input) const noexcept
        -> fixed_t<out_value_t, out_frac_bits>
    {
        using out_limits_t = std::numeric_limits<out_value_t>;

        auto const            int_part     = input.value >> in_frac_bits;
        static constexpr auto max_int_part = out_limits_t::digits - out_frac_bits;
        assert(int_part < max_int_part && "exp2_q32: integer overflow");
        if (int_part >= max_int_part) [[unlikely]] { return out_limits_t::max(); }

        static constexpr auto frac_mask = (std::make_unsigned_t<in_value_t>{1} << in_frac_bits) - 1;
        uint64_t              frac_part_q32;
        if constexpr (in_frac_bits > 32)
        {
            auto const shift = in_frac_bits - 32;
            auto const val   = static_cast<std::make_unsigned_t<in_value_t>>(input.value & frac_mask);
            frac_part_q32    = (val >> shift) + ((val >> (shift - 1)) & 1);
        }
        else if constexpr (in_frac_bits < 32)
        {
            frac_part_q32 = (static_cast<uint64_t>(input.value) & frac_mask) << (32 - in_frac_bits);
        }
        else
        {
            frac_part_q32 = static_cast<uint64_t>(input.value) & frac_mask;
        }

        auto accumulator = poly_coeffs[poly_degree];
        for (auto coeff_index = poly_degree - 1; coeff_index >= 1; --coeff_index)
        {
            auto const product = static_cast<uint128_t>(accumulator) * static_cast<uint128_t>(frac_part_q32);
            accumulator = static_cast<uint64_t>((product >> 32) + ((product >> 31) & 1) + poly_coeffs[coeff_index]);
        }

        auto const final_accumulator = static_cast<uint128_t>(accumulator) * static_cast<uint128_t>(frac_part_q32)
                                       + (static_cast<uint128_t>(poly_coeffs[0]) << 32);

        auto const final_shift = static_cast<int>(out_frac_bits) - 64 + static_cast<int>(int_part);

        if (final_shift >= 0) { return static_cast<out_value_t>(final_accumulator << final_shift); }
        else
        {
            auto const shr = -final_shift;
            if (shr > 128) [[unlikely]] { return 0; }
            return static_cast<out_value_t>((final_accumulator >> shr) + ((final_accumulator >> (shr - 1)) & 1));
        }
    }

private:
    static constexpr auto     poly_degree   = 7;
    static constexpr uint64_t poly_coeffs[] = {
        4294967296, 2977044495, 1031764415, 238393184, 41290194, 5767817, 614155, 93036,
    };
};

class exp2_minimax_t
{
public:
    template <unsigned_integral out_value_t, int_t out_frac_bits, integral in_value_t, int_t in_frac_bits>
    constexpr auto eval(fixed_t<in_value_t, in_frac_bits> input) const noexcept -> fixed_t<out_value_t, out_frac_bits>
    {
        using out_limits = std::numeric_limits<out_value_t>;

        // ------------------------------------------------------------------------------------------------------------
        // Reduce Range to [-0.5, 0.5]
        // ------------------------------------------------------------------------------------------------------------

        // extract signed int part, rounded half up
        //
        // This won't overflow for valid values, but because we do this before bounds checking, it may.
        constexpr auto half_bias = in_value_t(1) << (in_frac_bits - 1);
        auto const     int_part  = (input.value + half_bias) >> in_frac_bits;

        // extract signed frac part
        auto const frac_part = static_cast<int128_t>(input.value) - (static_cast<int128_t>(int_part) << in_frac_bits);

        // ------------------------------------------------------------------------------------------------------------
        // Bounds Check
        // ------------------------------------------------------------------------------------------------------------

        static constexpr auto max_out_int = out_limits::digits - out_frac_bits;
        auto const            overflows   = int_part >= max_out_int;
        if (overflows) [[unlikely]] { return out_limits::max(); }

        auto const underflows = int_part < -(int)out_frac_bits - 64;
        if (underflows) [[unlikely]] { return 0; }

        // ------------------------------------------------------------------------------------------------------------
        // Polynomial Evaluation
        // ------------------------------------------------------------------------------------------------------------R
        // evaluate exp2(frac_part) - 1 using Horner's method

        // We accumulate in signed 128-bit.
        // Coefficients are unsigned but treated as positive values in the signed accumulator.

        // Start with x^8 (Q.82)
        int128_t acc = poly_coeffs[0];

        // Horner's Loop: Process x^8 down to x^1
        // Loop runs 7 times (indices 0 to 6).
        // Adds coeffs[1] (x^7) through coeffs[7] (x^1).
        // #pragma unroll
        for (int i = 0; i < poly_degree - 1; ++i)
        {
            acc *= frac_part;

            // Shift = InputQ + DeltaQ
            int const shift = in_frac_bits + poly_shifts[i];

            // Rounding arithmetic right shift
            // acc = (acc + (int128_t(1) << (shift - 1))) >> shift; // may overflow
            acc = (acc >> shift) + ((acc >> (shift - 1)) & 1);

            acc += poly_coeffs[i + 1];
        }

        // Final Eval: Multiply by r one last time.
        // Loop ended after adding x^1 (Q.final_poly_shift).
        // So 'acc' is currently Q.final_poly_shift aligned.
        acc *= frac_part;

        // Result is now Q(Input + final_poly_shift).

        // --------------------------------------------------------------------
        // 4. Reconstruction (1.0 + acc) << k
        // --------------------------------------------------------------------
        // 1.0 in Q(Input + 6final_poly_shift3)
        int128_t const one             = int128_t(1) << (in_frac_bits + final_poly_shift);
        int128_t const result_unscaled = one + acc;

        // Final Shift: Convert Q(Input + final_poly_shift) -> Q(Output)
        // Adjust for exponent 'k' (int_part).
        // right_shift = current_Q - target_Q - k
        int const final_rshift = (in_frac_bits + final_poly_shift) - out_frac_bits - static_cast<int>(int_part);

        if (final_rshift >= 0)
        {
            if (final_rshift >= 128) [[unlikely]]
                return 0;

            // shr, round via carry
            return (static_cast<out_value_t>(result_unscaled >> final_rshift))
                   + (static_cast<out_value_t>((result_unscaled >> (final_rshift - 1)) & 1));
        }
        else
        {
            int const lshift = -final_rshift;
            if (lshift >= 128) [[unlikely]]
                return out_limits::max();
            return static_cast<out_value_t>(result_unscaled << lshift);
        }
    }

private:
    static constexpr auto     poly_degree   = 8;
    static constexpr uint64_t poly_coeffs[] = {
        6413836306507499907ULL, // 1.3263502612079305838139673820319920476140662657371649402193725109100341796875e-6*x^8
                                // (Q-19.82)
        4627166657175798541ULL, // 1.5310010199470489785574110451764513751715668377073598094284534454345703125e-5*x^7
                                // (Q-15.78)
        5819245827253372288ULL, // 1.5403415449549103224328831795997274412002298049628734588623046875e-4*x^6 (Q-12.75)
        6296544166165201635ULL, // 1.333345090645945436539436773464017971235762161086313426494598388671875e-3*x^5
                                // (Q-9.72)
        5677541381735370902ULL, // 9.61812921945912777541769049516329204152498277835547924041748046875e-3*x^4 (Q-6.69)
        8190960810162982905ULL, // 5.55041094070145845732204732680958869650567066855728626251220703125e-2*x^3 (Q-4.67)
        8862793787060165486ULL, // 0.2402265069555415648978012599368270230115740559995174407958984375*x^2 (Q-2.65)
        6393154322474104270ULL, // 0.69314718054615170341435648193595397970057092607021331787109375*x^1 (Q0.63)
    };
    static constexpr int_t poly_shifts[] = {
        4, // relative shift from x^8 (Q-19.82) to x^7 (Q-15.78)
        3, // relative shift from x^7 (Q-15.78) to x^6 (Q-12.75)
        3, // relative shift from x^6 (Q-12.75) to x^5 (Q-9.72)
        3, // relative shift from x^5 (Q-9.72) to x^4 (Q-6.69)
        2, // relative shift from x^4 (Q-6.69) to x^3 (Q-4.67)
        2, // relative shift from x^3 (Q-4.67) to x^2 (Q-2.65)
        2, // relative shift from x^2 (Q-2.65) to x^1 (Q0.63)
    };
    static constexpr auto final_poly_shift = 63;
};

} // namespace crv
