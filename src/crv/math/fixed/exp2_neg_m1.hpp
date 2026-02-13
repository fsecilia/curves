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

class exp2_neg_m1_q64_to_q1_63_t
{
public:
    // Input:  Q0.64 unsigned — x in [0, 1)
    // Output: Q1.63 unsigned — exp2(x) in (-0.5, 0]
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

        constexpr auto current_frac_bits = final_poly_shift + in_frac_bits;

        auto const rshift      = current_frac_bits - out_frac_bits;
        auto const acc_shifted = (acc >> rshift) + ((acc >> (rshift - 1)) & 1);
        return out_t{static_cast<int64_t>(acc_shifted)};
    }

private:
    // clang-format off
    // approx error: 5.91305734204227544246866890539576879751304864152136422586289765555174074535863104929437605857964576453846332049224991967833773502877957679321217037281569e-13
    static constexpr auto    poly_degree   = 8;
    static constexpr int64_t poly_coeffs[] = {
        9095336410655506548LL,  // 9.40435742942734746862840772884729358127575693515609600581228733062744140625e-7*x^8 (Q-20.83)
        -8818019289740466690LL, // -1.4588189195183866525152430586331243900222176534953177906572818756103515625e-5*x^7 (Q-16.79)
        5795025834175902226LL,  // 1.5339305661677577541916438750614626318480304689728654921054840087890625e-4*x^6 (Q-12.75)
        -6294873603319405290LL, // -1.33299133520323333419811185664161090613788474001921713352203369140625e-3*x^5 (Q-9.72)
        5677469737884917072LL,  // 9.61800785005552266748209133684355265359045006334781646728515625e-3*x^4 (Q-6.69)
        -8190957396465878113LL, // -5.55040862749032269297011672748443089631109614856541156768798828125e-2*x^3 (Q-4.67)
        8862793713294503620LL,  // 0.240226504956119293649093038300890157188405282795429229736328125*x^2 (Q-2.65)
        -6393154322035900772LL, // -0.6931471804986415849621217422082963821594603359699249267578125*x^1 (Q0.63)
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
