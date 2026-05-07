// SPDX-License-Identifier: MIT

/// \file
/// \brief polynomial defect analyzer
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/bitwise_enum.hpp>
#include <crv/math/spline/polynomial.hpp>

namespace crv {
namespace spline {

enum class segment_defects_t : int_t
{
    monotonicity = 1 << 0,
    overflow = 1 << 1,
};

} // namespace spline

template <> inline constexpr auto bitwise_for_enum_enabled<spline::segment_defects_t> = true;

namespace spline {

/// analyzes a polynomial for structural defects
template <typename monotonicity_check_t, typename overflow_check_t> struct defect_analyzer_t
{
    [[no_unique_address]] monotonicity_check_t check_monotonicity;
    [[no_unique_address]] overflow_check_t check_overflow;

    template <typename coeff_t>
    constexpr auto operator()(cubic_polynomial_t<coeff_t> const& polynomial) const noexcept -> segment_defects_t
    {
        auto result = segment_defects_t{};
        if (check_monotonicity(polynomial)) result |= segment_defects_t::monotonicity;
        if (check_overflow(polynomial)) result |= segment_defects_t::overflow;
        return result;
    }
};

} // namespace spline
} // namespace crv
