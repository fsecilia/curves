// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "defect_analyzer.hpp"
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

using coeff_t = fixed_t<int_t, 0>;
using polynomial_t = cubic_polynomial_t<coeff_t>;

constexpr auto expected_polynomial = polynomial_t{coeff_t{11}, coeff_t{13}, coeff_t{17}, coeff_t{19}};

// compile-time mock; throws if parameters are unexpected, returns prescribed result
template <bool result> struct checker_t
{
    constexpr auto operator()(polynomial_t const& polynomial) const -> bool
    {
        if (expected_polynomial != polynomial) throw "unexpected polynomial";
        return result;
    }
};

// test all combinations of nested checker results
static_assert(defect_analyzer_t<checker_t<false>, checker_t<false>>{}(expected_polynomial) == segment_defects_t{0});
static_assert(defect_analyzer_t<checker_t<false>, checker_t<true>>{}(expected_polynomial)
    == segment_defects_t{segment_defects_t::overflow});
static_assert(defect_analyzer_t<checker_t<true>, checker_t<false>>{}(expected_polynomial)
    == segment_defects_t{segment_defects_t::monotonicity});
static_assert(defect_analyzer_t<checker_t<true>, checker_t<true>>{}(expected_polynomial)
    == segment_defects_t{segment_defects_t::monotonicity | segment_defects_t::overflow});

} // namespace
} // namespace crv::spline
