// SPDX-License-Identifier: MIT
/**
    \file
    \brief autodifferentiating 1-jet

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>

namespace crv {

// --------------------------------------------------------------------------------------------------------------------
// Scalar Fallbacks
// --------------------------------------------------------------------------------------------------------------------

template <typename scalar_t> constexpr auto primal(scalar_t const& scalar) noexcept -> scalar_t
{
    return scalar;
}

template <typename scalar_t> constexpr auto derivative(scalar_t const&) noexcept -> scalar_t
{
    return scalar_t{};
}

} // namespace crv
