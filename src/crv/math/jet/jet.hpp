// SPDX-License-Identifier: MIT
/**
    \file
    \brief autodifferentiating 1-jet

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <compare>
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
    // Construction
    // ----------------------------------------------------------------------------------------------------------------

    constexpr jet_t() noexcept = default;
    constexpr jet_t(scalar_t const& s) noexcept : f{s}, df{} {}
    constexpr jet_t(scalar_t const& f, scalar_t const& df) noexcept : f{f}, df{df} {}

    /**
        broadcast ctor

        This ctor is deceptively simple, but it is the reason jet_t<jet_t<jet_t<jet_t<double>>>> + double works.

        Because of auto, this is a function template, so if you pass a scalar, the scalar ctor is a better match and
        this is not considered. So this doesn't affect normal usage. However, it forwards an arithmetic scalar to the
        nested f ctor. This recurses, and drills down until it hits a better scalar ctor. Since it's not explicit, this
        matches any scalar that the literal scalar ctor is not a better match for, and forwards it to the most nested
        jet in the composition.
    */
    constexpr jet_t(arithmetic auto const& s) noexcept : f(s), df(0) {}

    // ----------------------------------------------------------------------------------------------------------------
    // Conversions
    // ----------------------------------------------------------------------------------------------------------------

    template <typename other_scalar_t>
    explicit constexpr jet_t(jet_t<other_scalar_t> const& other) noexcept
        : f{static_cast<scalar_t>(other.f)}, df{static_cast<scalar_t>(other.df)}
    {}

    template <typename other_scalar_t> constexpr auto operator=(jet_t<other_scalar_t> const& rhs) noexcept -> jet_t&
    {
        f  = static_cast<scalar_t>(rhs.f);
        df = static_cast<scalar_t>(rhs.df);
        return *this;
    }

    explicit constexpr operator bool() const noexcept
    {
        // ignore derivative
        return f != scalar_t{0};
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Element Comparison
    // ----------------------------------------------------------------------------------------------------------------

    friend constexpr auto operator<=>(jet_t const& lhs, scalar_t const& rhs) noexcept
        -> std::common_comparison_category_t<decltype(lhs.f <=> rhs), std::weak_ordering>
    {
        return lhs.f <=> rhs;
    }

    friend constexpr auto operator==(jet_t const& lhs, scalar_t const& rhs) noexcept -> bool
    {
        return lhs.f == rhs && lhs.df == scalar_t{0};
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Vector Comparison
    // ----------------------------------------------------------------------------------------------------------------

    // jet_t ignores the derivative for ordering, so impose a weak ordering at best
    constexpr auto operator<=>(jet_t const& other) const noexcept
        -> std::common_comparison_category_t<decltype(f <=> other.f), std::weak_ordering>
    {
        return f <=> other.f;
    }

    constexpr auto operator==(jet_t const&) const noexcept -> bool = default;

    // ----------------------------------------------------------------------------------------------------------------
    // Accessors
    // ----------------------------------------------------------------------------------------------------------------

    friend constexpr auto primal(jet_t const& x) noexcept -> scalar_t { return x.f; }
    friend constexpr auto derivative(jet_t const& x) noexcept -> scalar_t { return x.df; }
};

} // namespace crv
