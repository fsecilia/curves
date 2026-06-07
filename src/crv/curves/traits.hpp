// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/complex_traits.hpp>

namespace crv {

//
// is_curve_scalar
//

// curves accept floating-point and std::complex as scalars; but not jets
template <typename value_t>
concept is_curve_scalar = std::floating_point<value_t> || is_complex<value_t>;

} // namespace crv
