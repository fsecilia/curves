// SPDX-License-Identifier: MIT
/**
    \file
    \brief autodifferentiating 1-jet

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <cmath>
#include <compare>
#include <concepts>
#include <ostream>
#include <type_traits>

namespace crv {

using std::isfinite;
using std::isinf;
using std::isnan;

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

        Because of auto, this is a function template, so if you pass a scalar_t, the scalar ctor is a better match and
        this is not considered. So this doesn't affect normal usage. However, if you pass an arithmetic other than
        scalar_t, it forwards to the nested f ctor. This recurses, drilling down until it finds a leaf, then invokes
        either the better scalar ctor, or the conversion ctor. Since it's not explicit, this matches any scalar that the
        literal scalar ctor is not a better match for, and forwards it to the most nested jet in the composition.
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

    // ----------------------------------------------------------------------------------------------------------------
    // Unary Arithmetic
    // ----------------------------------------------------------------------------------------------------------------

    friend constexpr auto operator+(jet_t const& x) noexcept -> jet_t { return x; }
    friend constexpr auto operator-(jet_t const& x) noexcept -> jet_t { return {-x.f, -x.df}; }

    // ----------------------------------------------------------------------------------------------------------------
    // Scalar Arithmetic
    // ----------------------------------------------------------------------------------------------------------------

    constexpr auto operator+=(scalar_t const& rhs) noexcept -> jet_t&
    {
        f += rhs;
        return *this;
    }

    friend constexpr auto operator+(jet_t lhs, scalar_t const& rhs) noexcept -> jet_t { return lhs += rhs; }
    friend constexpr auto operator+(scalar_t const& lhs, jet_t rhs) noexcept -> jet_t
    {
        // commute
        return rhs += lhs;
    }

    constexpr auto operator-=(scalar_t const& rhs) noexcept -> jet_t&
    {
        f -= rhs;
        return *this;
    }

    friend constexpr auto operator-(jet_t lhs, scalar_t const& rhs) noexcept -> jet_t { return lhs -= rhs; }
    friend constexpr auto operator-(scalar_t const& lhs, jet_t const& rhs) noexcept -> jet_t
    {
        return jet_t{lhs - rhs.f, -rhs.df};
    }

    constexpr auto operator*=(scalar_t const& x) noexcept -> jet_t&
    {
        f *= x;
        df *= x;
        return *this;
    }

    friend constexpr auto operator*(jet_t lhs, scalar_t const& rhs) noexcept -> jet_t { return lhs *= rhs; }
    friend constexpr auto operator*(scalar_t const& lhs, jet_t rhs) noexcept -> jet_t
    {
        // commute
        return rhs *= lhs;
    }

    constexpr auto operator/=(scalar_t const& x) noexcept -> jet_t&
    {
        auto const inv = scalar_t(1.0) / x;
        f *= inv;
        df *= inv;
        return *this;
    }

    friend constexpr auto operator/(jet_t lhs, scalar_t const& rhs) noexcept -> jet_t { return lhs /= rhs; }
    friend constexpr auto operator/(scalar_t const& lhs, jet_t rhs) noexcept -> jet_t
    {
        auto const inv = scalar_t(1.0) / rhs.f;
        return jet_t{lhs, -lhs * rhs.df * inv} * inv;
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Vector Arithmetic
    // ----------------------------------------------------------------------------------------------------------------

    constexpr auto operator+=(jet_t const& rhs) noexcept -> jet_t&
    {
        f += rhs.f;
        df += rhs.df;
        return *this;
    }

    friend constexpr auto operator+(jet_t lhs, jet_t const& rhs) noexcept -> jet_t { return lhs += rhs; }

    constexpr auto operator-=(jet_t const& rhs) noexcept -> jet_t&
    {
        f -= rhs.f;
        df -= rhs.df;
        return *this;
    }

    friend constexpr auto operator-(jet_t lhs, jet_t const& rhs) noexcept -> jet_t { return lhs -= rhs; }

    // d(xy) = x*dy + dx*y, product rule
    constexpr auto operator*=(jet_t const& x) noexcept -> jet_t&
    {
        // product rule, (uv)' = uv' + u'v:
        df = f * x.df + df * x.f;
        f *= x.f;
        return *this;
    }

    friend constexpr auto operator*(jet_t lhs, jet_t const& rhs) noexcept -> jet_t { return lhs *= rhs; }

    // d(u/v) = (du*v - u*dv)/v^2 = (du - u*dv/v)/v, quotient rule
    constexpr auto operator/=(jet_t const& x) noexcept -> jet_t&
    {
        /*
            This looks suspicious because we modify f then use it to calc df, but it's a deliberate optimization,
            similar to horner's.
        */
        auto const inv = scalar_t(1.0) / x.f;
        f *= inv;
        df = (df - f * x.df) * inv;
        return *this;
    }

    friend constexpr auto operator/(jet_t lhs, jet_t const& rhs) noexcept -> jet_t { return lhs /= rhs; }

    // ----------------------------------------------------------------------------------------------------------------
    // Selection
    // ----------------------------------------------------------------------------------------------------------------

    // d(min(x, y)) = dx if x < y else dy
    friend constexpr auto min(jet_t const& x, jet_t const& y) noexcept -> jet_t { return x.f < y.f ? x : y; }

    // d(max(x, y)) = dx if y < x else dy
    friend constexpr auto max(jet_t const& x, jet_t const& y) noexcept -> jet_t { return y.f < x.f ? x : y; }

    // d(clamp(x, min, max)) = min.df if x < min else max.df if x > max else dx
    friend constexpr auto clamp(jet_t const& x, jet_t const& min, jet_t const& max) noexcept -> jet_t
    {
        if (x.f < min.f) return min;
        if (x.f > max.f) return max;
        return x;
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Classification
    // ----------------------------------------------------------------------------------------------------------------

    friend auto isfinite(jet_t const& x) noexcept -> bool
    {
        using crv::isfinite;
        return isfinite(x.f) && isfinite(x.df);
    }

    friend auto isinf(jet_t const& x) noexcept -> bool
    {
        using crv::isinf;
        return (isinf(x.f) || isinf(x.df)) && !isnan(x);
    }

    friend auto isnan(jet_t const& x) noexcept -> bool
    {
        using crv::isnan;
        return isnan(x.f) || isnan(x.df);
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Standard Library Integration
    // ----------------------------------------------------------------------------------------------------------------

    friend auto operator<<(std::ostream& out, jet_t const& src) -> std::ostream&
    {
        return out << "{.f = " << src.f << ", .df = " << src.df << "}";
    }
};

} // namespace crv
