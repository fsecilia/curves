// SPDX-License-Identifier: MIT
/**
    \file
    \brief autodifferentiating 1-jet

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <type_traits>

namespace crv {

// --------------------------------------------------------------------------------------------------------------------
// Concepts
// --------------------------------------------------------------------------------------------------------------------

template <typename scalar_t>
concept arithmetic = std::is_arithmetic_v<scalar_t>;

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

// --------------------------------------------------------------------------------------------------------------------
// Jet
// --------------------------------------------------------------------------------------------------------------------

template <typename t_scalar_t> struct jet_t
{
    using scalar_t = t_scalar_t;

    scalar_t f{};
    scalar_t df{};

    // ----------------------------------------------------------------------------------------------------------------
    // Accessors
    // ----------------------------------------------------------------------------------------------------------------

    friend constexpr auto primal(jet_t const& x) noexcept -> scalar_t { return x.f; }
    friend constexpr auto derivative(jet_t const& x) noexcept -> scalar_t { return x.df; }
};

} // namespace crv
