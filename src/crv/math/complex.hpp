// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/complex_traits.hpp>

namespace crv {

// partial implementation of complex erf supporting only enough for complex-step
//
// This is *not* complex erf. It is erf with a complex approxiation within epsilon of the real line. It is just enough
// to run complex-step tests. A proper implementation requires the Faddeeva function, which is well beyond the scope of
// this project.
//
// Noncomplex value_t delegates to erf via ADL. On the complex path, the input must z = a + i*b with |b| infinitesimal,
// then this function returns real erf(x) in the real part and the exact analytic first-order tangent in the imaginary
// part:
//
//     b*erf'(a) = b*(2/sqrt(pi))*e^{-a^2}
//
// This is what Im(f(x+ih))/h reads back when applying complex-step. It is not erf off the real axis. For any finite b,
// the imaginary part is wrong. It is correct solely for the infinitesimal b inputs complex-step produces.
template <typename value_t> auto complex_step_erf(value_t z) noexcept -> value_t
{
    using std::erf;
    using std::exp;

    if constexpr (is_complex<value_t>)
    {
        using real_type_t = real_type_t<value_t>;
        auto const a = z.real();
        auto const b = z.imag();

        static constexpr auto two_over_sqrt_pi = real_type_t{2} * std::numbers::inv_sqrtpi_v<real_type_t>;
        return value_t{erf(a), b * static_cast<real_type_t>(two_over_sqrt_pi) * exp(-a * a)};
    }
    else
    {
        return erf(z);
    }
}

} // namespace crv
