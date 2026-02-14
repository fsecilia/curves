// SPDX-License-Identifier: MIT
/**
    \file
    \brief fixed point integer type

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>

namespace crv {

/// executes horner's loop directly on reduced minimax approximation of 2^-x - 1
class exp2_neg_m1_q64_to_q1_63_t
{
public:
    // input:  Q0.64 unsigned — x in [0, 1)
    // output: Q1.63 unsigned — exp2(x) in (-0.5, 0]
    using in_t  = fixed_t<uint64_t, 64>;
    using out_t = fixed_t<int64_t, 63>;

    static constexpr auto in_frac_bits  = in_t::frac_bits;
    static constexpr auto out_frac_bits = out_t::frac_bits;

    constexpr auto eval(in_t const& input) const noexcept -> out_t
    {
        auto acc = static_cast<int128_t>(poly_coeffs[0]);
        for (auto i = 0; i < poly_degree - 1; ++i)
        {
            acc *= static_cast<int128_t>(input.value);

            auto const shift = in_frac_bits + poly_shifts[i];

            acc = (acc >> shift) + ((acc >> (shift - 1)) & 1);
            acc += static_cast<int128_t>(poly_coeffs[i + 1]);
        }

        acc *= static_cast<int128_t>(input.value);

        auto const shift = final_poly_shift + in_frac_bits - out_frac_bits;

        acc = (acc >> shift) + ((acc >> (shift - 1)) & 1);
        return out_t{static_cast<int64_t>(acc)};
    }

private:
    // clang-format off
    // approx error: 6.323171829167966859883343079686165349076998875349194105818490329619770045738686142167850793114206798549808944399318145087994841745905878981650675275195845e-13
    static constexpr auto poly_degree = 8;
    static constexpr int64_t poly_coeffs[] = {
        9071579751494610204LL, // 9.37979361958119280078222574864132803085237100049198488704860210418701171875e-7*x^8 (Q-20.83)
        -8812490343530759173LL, // -1.45790423209588293161126225060673401723310149691315018571913242340087890625e-5*x^7 (Q-16.79)
        5794507077935159199LL, // 1.53379325253416302684244845971721360466943906430969946086406707763671875e-4*x^6 (Q-12.75)
        -6294823422682983937LL, // -1.332980709040142565418568380321284738698750516050495207309722900390625e-3*x^5 (Q-9.72)
        5677467086801719021LL, // 9.618003358945893344493006417439406874336782493628561496734619140625e-3*x^4 (Q-6.69)
        -8190957247748662557LL, // -5.55040852671561757308870722893434646039168001152575016021728515625e-2*x^3 (Q-4.67)
        8862793709373760939LL, // 0.24022650484984735033667201109519595547681092284619808197021484375*x^2 (Q-2.65)
        -6393154322001310877LL, // -0.693147180494891340977507587289011131161629852837080534300184808671474456787109375*x^1 (Q0.63)
    };
    static constexpr int_t poly_shifts[] = {
        4, // relative shift from x^8 (Q-20.83) to x^7 (Q-16.79)
        4, // relative shift from x^7 (Q-16.79) to x^6 (Q-12.75)
        3, // relative shift from x^6 (Q-12.75) to x^5 (Q-9.72)
        3, // relative shift from x^5 (Q-9.72) to x^4 (Q-6.69)
        2, // relative shift from x^4 (Q-6.69) to x^3 (Q-4.67)
        2, // relative shift from x^3 (Q-4.67) to x^2 (Q-2.65)
        2, // relative shift from x^2 (Q-2.65) to x^1 (Q0.63)
    };
    static constexpr auto final_poly_shift = 63;
    // clang-format on
};

} // namespace crv
