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

    auto operator<=>(segment_t const&) const noexcept -> auto = default;
    auto operator==(segment_t const&) const noexcept -> bool  = default;
};

template <std::floating_point real_t> struct bisection_t
{
    segment_t<real_t> left_child;
    segment_t<real_t> right_child;
    real_t            combined_integral;
    real_t            error_estimate;

    auto operator<=>(bisection_t const&) const noexcept -> auto = default;
    auto operator==(bisection_t const&) const noexcept -> bool  = default;
};

} // namespace crv::quadrature
