// SPDX-License-Identifier: MIT

/// \file
/// \brief fixed point approximation of 2^-x - 1
/// \copyright Copyright (C) 2026 Frank Secilia

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
    using in_t = fixed_t<uint64_t, 64>;
    using out_t = fixed_t<int64_t, 63>;

    static constexpr auto in_frac_bits = in_t::frac_bits;
    static constexpr auto out_frac_bits = out_t::frac_bits;

    static auto prefetch() noexcept -> void
    {
        constexpr auto read_only = 0;
        constexpr auto high_locality = 3; // Keep in L1
        __builtin_prefetch(poly_coeffs, read_only, high_locality);
    }

    constexpr auto operator()(in_t input) const noexcept -> out_t
    {
        auto acc = poly_coeffs[0];

        for (auto i = 0; i < poly_degree - 1; ++i)
        {
            auto const product = static_cast<int128_t>(acc) * static_cast<int128_t>(input.value);
            auto const shift = in_frac_bits + poly_shifts[i];
            acc = static_cast<int64_t>((product >> shift) + ((product >> (shift - 1)) & 1)) + poly_coeffs[i + 1];
        }

        auto const final_product = static_cast<int128_t>(acc) * static_cast<int128_t>(input.value);
        auto const final_shift = 64 + poly_shifts[poly_degree - 1] + in_frac_bits - out_frac_bits;
        acc = static_cast<int64_t>((final_product >> final_shift) + ((final_product >> (final_shift - 1)) & 1));

        return out_t::literal(static_cast<int64_t>(acc));
    }

private:
    // clang-format off
    // approx error: 6.323171829167966859883343079686165349076998875349194105818490329619770045738686142167850793114206798549808944399318145087994841745905878981650675275195845e-13
    static constexpr auto poly_degree = 8;
    static const int64_t poly_coeffs[];
    static constexpr int_t poly_shifts[] = {
        4, // relative shift from x^8 (Q-20.83) to x^7 (Q-16.79)
        4, // relative shift from x^7 (Q-16.79) to x^6 (Q-12.75)
        3, // relative shift from x^6 (Q-12.75) to x^5 (Q-9.72)
        3, // relative shift from x^5 (Q-9.72) to x^4 (Q-6.69)
        2, // relative shift from x^4 (Q-6.69) to x^3 (Q-4.67)
        2, // relative shift from x^3 (Q-4.67) to x^2 (Q-2.65)
        2, // relative shift from x^2 (Q-2.65) to x^1 (Q0.63)
        -1, // relative shift from 128-bits to 64-bits
    };
    // clang-format on
};

} // namespace crv
