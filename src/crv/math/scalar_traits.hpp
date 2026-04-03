// SPDX-License-Identifier: MIT

/// \file
/// \brief baseline scalar traits to support generic algorithms
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>

namespace crv {

// --------------------------------------------------------------------------------------------------------------------
// Jet Fallbacks
// --------------------------------------------------------------------------------------------------------------------

template <typename scalar_t> constexpr auto primal(scalar_t scalar) noexcept -> scalar_t
{
    return scalar;
}

template <typename scalar_t> constexpr auto derivative(scalar_t) noexcept -> scalar_t
{
    return scalar_t{};
}

} // namespace crv
