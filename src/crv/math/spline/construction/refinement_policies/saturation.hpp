// SPDX-License-Identifier: MIT

/// \file
/// \brief saturation check
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <concepts>
#include <iterator>

namespace crv::spline::refinement_policies {

template <typename coeffs_t>
concept is_monomial = requires(coeffs_t coeffs) {
    typename coeffs_t::value_type;
    requires is_fixed<typename coeffs_t::value_type>;
    { coeffs.size() } -> std::convertible_to<std::size_t>;
    { coeffs[0] };
    { coeffs[coeffs.size() - 1] };
};

/// predicate returning true if approximant saturates while evaluating anywhere over the interval
template <typename real_t, is_fixed normalized_t> struct saturation_t
{
    template <is_monomial monomial_t> constexpr auto operator()(monomial_t const& coeffs) const noexcept -> bool
    {
        // test endpoints; this covers the linear intermediate in all cases
        if (operator()(coeffs, normalized_t{0})) return true;
        if (operator()(coeffs, normalized_t{1})) return true; // this pops if the type has no integer bits

        // scale cancels out when f'(t) = 0, so cast and avoid division
        auto const a = static_cast<real_t>(coeffs[0].value);
        auto const b = static_cast<real_t>(coeffs[1].value);
        auto const c = static_cast<real_t>(coeffs[2].value);

        // find largest term
        if (a)
        {
            // The cubic term is valid, so first check the absolute maximum the quadratic from the previous horner's
            // loop step could ever reach. If the full cubic is p(t) = at^3 + bt^2 + ct + d, the intermediate quadratic
            // from the previous step is q(t) = at^2 + bt + c. The peak is at q'(t) = 2at + b = 0 -> -b/2a
            auto const t_quadratic = -b / (2.0 * a);
            if (0.0 < t_quadratic && t_quadratic < 1.0 && operator()(coeffs, to_fixed<normalized_t>(t_quadratic)))
            {
                return true;
            }

            // The peaks of the final cubic are at p'(t) = 3at^2 + 2bt + c. Using the quadratic equation, the roots are
            // t = (-2b +/- sqrt(4b^2 - 12ac))/6a = (-b +/- sqrt(b^2 - 3ac))/3a
            auto const discriminant = b * b - 3.0 * a * c;
            auto const r3a = 1.0 / (3.0 * a);
            if (discriminant > 0.0)
            {
                using std::sqrt;
                auto const sqrt_discriminant = sqrt(discriminant);

                auto const t0 = (-b - sqrt_discriminant) * r3a;
                if (0.0 < t0 && t0 < 1.0 && operator()(coeffs, to_fixed<normalized_t>(t0))) return true;

                auto const t1 = (-b + sqrt_discriminant) * r3a;
                if (0.0 < t1 && t1 < 1.0 && operator()(coeffs, to_fixed<normalized_t>(t1))) return true;
            }
        }
        else if (b)
        {
            // The cubic term is invalid, but the quadratic term is valid. The full quadratic is p(t) = bt^2 + ct + d.
            // The peak of the final quadratic is at p'(t) = 2bt + c = 0 -> -c/2b
            auto const t = -c / (2.0 * b);
            if (0.0 < t && t < 1.0 && operator()(coeffs, to_fixed<normalized_t>(t))) return true;
        }

        return false;
    }

    template <is_monomial monomial_t>
    constexpr auto operator()(monomial_t const& coeffs, normalized_t t) const noexcept -> bool
    {
        using coeff_t = monomial_t::value_type;
        auto accumulator = coeffs[0];
        for (auto coeff = 1u, coeff_count = std::size(coeffs); coeff < coeff_count; ++coeff)
        {
            auto wide = multiply(accumulator, t) >> normalized_t::frac_bits;
            wide.value += coeffs[coeff].value;
            if (wide.value < min<coeff_t>().value || max<coeff_t>().value < wide.value) return true;
            accumulator.value = wide.value;
        }
        return false;
    }
};

} // namespace crv::spline::refinement_policies
