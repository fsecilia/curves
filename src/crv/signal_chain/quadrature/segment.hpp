// SPDX-License-Identifier: MIT

/// \file
/// \brief segment types
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <concepts>

namespace crv::quadrature {

/// interval plus its level-N rule estimate
///
/// coarse_integral is one rule evaluation across [left, right]. It is the comparison baseline for subdivision_error in
/// the next refinement.
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

/// the result of refining a parent segment
///
/// left and right are the children at level N+1, each with their own coarse_integral. refined_integral and
/// refined_error are the parent's level-N+1 estimate and error, the sum of the children's rule evaluations.
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
