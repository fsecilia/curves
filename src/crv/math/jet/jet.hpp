// SPDX-License-Identifier: MIT
/**
    \file
    \brief autodifferentiating 1-jet

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <crv/math/limits.hpp>
#include <cassert>
#include <cmath>
#include <compare>
#include <concepts>
#include <ostream>
#include <type_traits>

namespace crv {

using std::abs;
using std::clamp;
using std::copysign;
using std::cos;
using std::exp;
using std::hypot;
using std::isfinite;
using std::isinf;
using std::isnan;
using std::log;
using std::log1p;
using std::max;
using std::min;
using std::pow;
using std::sin;
using std::sqrt;
using std::tan;

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
    // Math Functions
    // ----------------------------------------------------------------------------------------------------------------

    /// d(abs(x)) = sgn(x)*dx
    friend constexpr auto abs(jet_t const& x) noexcept -> jet_t
    {
        using crv::abs;

        return {abs(x.f), copysign(scalar_t{1}, x.f) * x.df};
    }

    /**
        applies sign of y to magnitude of x

        copysign(x, y) = |x|*sgn(y)
        d(copysign(x, y)) = d(|x|)*sgn(y) + |x|*d(sgn(y)) = (sgn(x)*dx)*sgn(y) + |x|*(delta(y)*dy) ; product rule

        The dy term has a jump discontinuity at y = 0, producing a Dirac delta in the derivative. Returns df = +/- inf
        when y crosses zero with nonzero |x|.
    */
    friend constexpr auto copysign(jet_t const& x, jet_t const& y) noexcept -> jet_t
    {
        using crv::copysign;

        auto const sgn_x = copysign(scalar_t{1}, x.f);
        auto const sgn_y = copysign(scalar_t{1}, y.f);

        auto const dx_term = sgn_x * sgn_y * x.df;

        /*
            handle dirac delta spike on y

            When y.f != 0, the spike is 0. When y.f == 0, the spike MAY be inf:
                - When x.f is 0, the function is continuous at 0, as the jump height is 0, so the spike is 0.
                - When y.df == 0, y is a constant, so there is no delta contribution. However, if we scaled inf by 0,
                  the result would be NaN, so only apply inf conditionally.
                - When we know x.f != 0 and delta(y) = inf, |x|*(delta(y)*dy) == inf*dy.
        */
        auto const has_delta = (y.f == scalar_t{0}) & (x.f != scalar_t{0}) & (y.df != scalar_t{0});
        auto const dy_term   = has_delta ? infinity<scalar_t>() * y.df : scalar_t{0};

        return {copysign(x.f, y.f), dx_term + dy_term};
    }

    // d(cos(x)) = -sin(x)*dx
    friend constexpr auto cos(jet_t const& x) noexcept -> jet_t
    {
        using crv::cos;
        using crv::sin;

        return {cos(x.f), -sin(x.f) * x.df};
    }

    // d(exp(x)) = exp(x)*dx
    friend constexpr auto exp(jet_t const& x) noexcept -> jet_t
    {
        using crv::exp;

        auto const exp_xf = exp(x.f);
        return {exp_xf, exp_xf * x.df};
    }

    // d(hypot(x, y)) = (x*dx + y*dy) / hypot(x, y)
    friend auto hypot(jet_t const& x, jet_t const& y) noexcept -> jet_t
    {
        using crv::hypot;

        auto const mag = hypot(x.f, y.f);
        if (mag == scalar_t{0}) return {scalar_t{0}, scalar_t{0}};

        return {mag, (x.f * x.df + y.f * y.df) / mag};
    }

    //! \pre x > 0
    // d(log(x)) = dx/x
    friend constexpr auto log(jet_t const& x) noexcept -> jet_t
    {
        using crv::log;

        assert(x.f > scalar_t{0} && "jet_t::log: domain error");

        return {log(x.f), x.df / x.f};
    }

    //! \pre x > -1
    // d(log1p(x)) = dx/(x + 1)
    friend constexpr auto log1p(jet_t const& x) noexcept -> jet_t
    {
        using crv::log1p;

        assert(x.f > scalar_t{-1} && "jet_t::log1p: domain error");

        return {log1p(x.f), x.df / (x.f + 1)};
    }

    //! \pre x > 0 || (x == 0 && y >= 1)
    // jet^element
    // d(x^y) = x^(y - 1)*y*dx
    friend constexpr auto pow(jet_t const& x, scalar_t const& y) noexcept -> jet_t
    {
        using crv::pow;

        // We restrict the range to positive numbers or 0 with a positive exponent.
        //
        // x < 0:
        // The vast majority of the domain has nonreal results and we don't support complex jets. The only real results
        // come from negative integers, which don't come up in our usage. Instead of bothering with an int check, all of
        // x < 0 is excluded.
        //
        // x == 0:
        // The result is inf if y < 1.
        assert((x > 0 || (x == 0 && y >= 1)) && "jet_t::pow(<jet>, <element>): domain error");

        auto const pm1 = pow(x.f, y - scalar_t(1));
        return {pm1 * x.f, y * pm1 * x.df};
    }

    //! \pre x > 0
    // element^jet
    // d(x^y) = log(x)*x^y*dy
    friend constexpr auto pow(scalar_t const& x, jet_t const& y) noexcept -> jet_t
    {
        using crv::pow;

        assert(x > scalar_t(0) && "jet_t::pow(<element>, <jet>): domain error");

        auto const power    = pow(x, y.f);
        auto const log_base = log(x);

        return {power, log_base * power * y.df};
    }

    //! \pre x > 0
    // jet^jet
    // d(x^y) = x^y*(log(x)*dy + y*dx/x) = x^y*log(x)*dy + x^(y - 1)*y*dx
    friend constexpr auto pow(jet_t const& x, jet_t const& y) noexcept -> jet_t
    {
        using crv::pow;

        assert(x.f > scalar_t{0} && "jet_t::pow(<jet>, <jet>): domain error");

        /*
            By definition:

                x^y = e^(log(x)*y)
                d(e^(f(x))) = e^(f(x))d(f(x))

            Here, f(x) = log(x)*y:

                d(f(x)) = log(x)*d(y) + d(log(x))*y
                        = log(x)*dy + y*dx/x

            Using this, the full derivation is:

                d(x^y) = e^(log(x)*y)(log(x)*dy + y*dx/x)
                       = (x^y)(log(x)*dy + y*dx/x)
                       = x^y*log(x)*dy + x^(y - 1)*y*dx

            The familiar power rule is recovered when y is a constant because that makes dy = 0.
        */
        auto const pm1   = pow(x.f, y.f - 1);
        auto const power = x.f * pm1;
        return {power, power * log(x.f) * y.df + pm1 * y.f * x.df};
    }

    // d(cos(x)) = cos(x)*dx
    friend constexpr auto sin(jet_t const& x) noexcept -> jet_t
    {
        using crv::cos;
        using crv::sin;

        return {sin(x.f), cos(x.f) * x.df};
    }

    // d(sqrt(x)) = dx/(2*sqrt(x))
    friend constexpr auto sqrt(jet_t const& x) noexcept -> jet_t
    {
        using crv::sqrt;

        assert(x.f >= scalar_t{0} && "jet_t::sqrt domain error");

        auto const root = sqrt(x.f);
        if (root == scalar_t{0}) return {scalar_t{0}, infinity<scalar_t>()};

        return {root, x.df / (scalar_t{2} * root)};
    }

    // d(tan(x)) = (1 + tan(x)^2)*dx
    friend constexpr auto tan(jet_t const& x) noexcept -> jet_t
    {
        using crv::tan;

        auto const tan_f = tan(x.f);
        return {tan_f, (scalar_t{1} + tan_f * tan_f) * x.df};
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
