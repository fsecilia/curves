// SPDX-License-Identifier: MIT

/// \file
/// \brief cubic monomial
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <array>

namespace crv::spline {

constexpr auto cubic_coeff_count = 4;
template <is_fixed coeff_t> using cubic_monomial_t = std::array<coeff_t, cubic_coeff_count>;

} // namespace crv::spline
