// SPDX-License-Identifier: MIT
/**
    \file
    \brief autodifferentiating 1-jet

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <concepts>
#include <type_traits>

namespace crv {

// --------------------------------------------------------------------------------------------------------------------
// Concepts
// --------------------------------------------------------------------------------------------------------------------

template <typename scalar_t>
concept arithmetic = std::is_arithmetic_v<scalar_t>;

template <typename jet_t>
concept is_jet = !std::same_as<jet_t, decltype(primal(std::declval<jet_t const>()))>;

template <typename jet_t>
concept is_not_jet = !is_jet<jet_t>;

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
    // Constructors
    // ----------------------------------------------------------------------------------------------------------------

    constexpr jet_t() noexcept = default;
    constexpr jet_t(scalar_t const& f, scalar_t const& df) noexcept : f{f}, df{df} {}

    // ----------------------------------------------------------------------------------------------------------------
    // Accessors
    // ----------------------------------------------------------------------------------------------------------------

    friend constexpr auto primal(jet_t const& x) noexcept -> scalar_t { return x.f; }
    friend constexpr auto derivative(jet_t const& x) noexcept -> scalar_t { return x.df; }
};

} // namespace crv
