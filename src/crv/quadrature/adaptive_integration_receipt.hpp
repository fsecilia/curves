// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <concepts>

namespace crv::quadrature {

/// adaptive quadrature construction receipt
///
/// This type contains error metrics describing the quality of the integral approximation.
template <std::floating_point t_scalar_t> struct adaptive_integration_receipt_t
{
    using scalar_t = t_scalar_t;

    scalar_t requested_tolerance;
    scalar_t achieved_error;
    scalar_t max_error;
    int_t segment_count;
    bool refinement_limited = false;

    constexpr auto operator==(adaptive_integration_receipt_t const&) const noexcept -> bool = default;
};

} // namespace crv::quadrature
