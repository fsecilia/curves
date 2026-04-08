// SPDX-License-Identifier: MIT

/// \file
/// \brief segment types
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <concepts>

namespace crv::quadrature {

template <std::floating_point real_t> struct segment_t
{
    real_t left;
    real_t right;
    real_t integral;
    real_t tolerance;
    int_t  depth;
};

template <std::floating_point real_t> struct bisection_t
{
    segment_t<real_t> left_child;
    segment_t<real_t> right_child;
    real_t            combined_integral;
    real_t            error_estimate;
};

} // namespace crv::quadrature
