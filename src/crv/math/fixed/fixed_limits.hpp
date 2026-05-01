// SPDX-License-Identifier: MIT

/// \file
/// \brief fixed point numeric limits
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <limits>

namespace std {

template <typename value_t, int frac_bits>
struct numeric_limits<crv::fixed_t<value_t, frac_bits>> : numeric_limits<value_t>
{
    using base_t = numeric_limits<value_t>;
    using fixed_t = crv::fixed_t<value_t, frac_bits>;

    static constexpr bool is_specialized = true;

    // fixed-point represents fractions, but does so exactly
    static constexpr bool is_integer = false;
    static constexpr bool is_exact = true;

    // fixed-point lacks float special values
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;

    // smallest strictly positive value is 1 in the underlying
    static constexpr auto min() noexcept -> fixed_t { return fixed_t::literal(1); }

    // max and lowest wrap the underlying integer's bounds
    static constexpr auto max() noexcept -> fixed_t { return fixed_t::literal(base_t::max()); }
    static constexpr auto lowest() noexcept -> fixed_t { return fixed_t::literal(base_t::lowest()); }

    // the difference between 1.0 and the next representable value is always 1
    static constexpr auto epsilon() noexcept -> fixed_t { return fixed_t::literal(1); }
    static constexpr auto round_error() noexcept -> fixed_t { return fixed_t::literal(1); }

    // Return zero-initialized representations for unsupported float concepts
    static constexpr auto infinity() noexcept -> fixed_t { return fixed_t::literal(0); }
    static constexpr auto quiet_NaN() noexcept -> fixed_t { return fixed_t::literal(0); }
    static constexpr auto signaling_NaN() noexcept -> fixed_t { return fixed_t::literal(0); }
    static constexpr auto denorm_min() noexcept -> fixed_t { return min(); }
};

} // namespace std
