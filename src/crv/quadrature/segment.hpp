// SPDX-License-Identifier: MIT

/// \file
/// \brief segment types
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <concepts>

namespace crv::quadrature {

/// interval plus its current rule estimate
///
/// coarse_integral is one rule evaluation over [left, right] and becomes the parent baseline on the next refinement.
template <std::floating_point t_scalar_t> struct segment_t
{
    using scalar_t = t_scalar_t;

    scalar_t left;
    scalar_t right;
    scalar_t coarse_integral;
    scalar_t tolerance;
    int_t depth;

    auto operator<=>(segment_t const&) const noexcept -> auto = default;
    auto operator==(segment_t const&) const noexcept -> bool = default;
};

/// result of refining a parent segment
///
/// left and right hold the child rule estimates. refined_integral is their sum, and refined_error is the resulting
/// parent error estimate.
template <std::floating_point t_scalar_t> struct refinement_t
{
    using scalar_t = t_scalar_t;
    using segment_t = segment_t<scalar_t>;

    segment_t left;
    segment_t right;
    scalar_t refined_integral;
    scalar_t refined_error;

    auto operator<=>(refinement_t const&) const noexcept -> auto = default;
    auto operator==(refinement_t const&) const noexcept -> bool = default;
};

} // namespace crv::quadrature
