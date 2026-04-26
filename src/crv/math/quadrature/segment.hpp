// SPDX-License-Identifier: MIT

/// \file
/// \brief segment types
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <concepts>

namespace crv::quadrature {

template <std::floating_point t_real_t> struct segment_t
{
    using real_t = t_real_t;

    real_t left;
    real_t right;
    real_t coarse_integral;
    real_t tolerance;
    int_t depth;

    auto operator<=>(segment_t const&) const noexcept -> auto = default;
    auto operator==(segment_t const&) const noexcept -> bool = default;
};

template <std::floating_point t_real_t> struct refinement_t
{
    using real_t = t_real_t;
    using segment_t = segment_t<real_t>;

    segment_t left;
    segment_t right;
    real_t refined_integral;
    real_t error_estimate;

    auto operator<=>(refinement_t const&) const noexcept -> auto = default;
    auto operator==(refinement_t const&) const noexcept -> bool = default;
};

} // namespace crv::quadrature
