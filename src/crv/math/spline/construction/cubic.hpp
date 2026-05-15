// SPDX-License-Identifier: MIT

/// \file
/// \brief cubic spline polynomial
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/polynomial.hpp>

namespace crv::spline {

template <std::floating_point scalar_t> using cubic_t = polynomial_t<scalar_t, 3>;

} // namespace crv::spline
