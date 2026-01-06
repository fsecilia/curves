// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Autodiffing jet implementation.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <cassert>
#include <cmath>
#include <compare>
#include <limits>
#include <ostream>

// This is needed by HasCusp, which should not be here.
// This module used to be more generic curve abstractions.
#include <concepts>

namespace curves {

/*!
  Results of f(x) and f'(x).

  This will eventually support autodifferentiation using dual numbers, but for
  now, it's just the function and its derivative.
*/
struct Jet {
  real_t f;
  real_t df;
};

template <typename Curve>
concept HasCusp = requires(Curve curve) {
  { curve.cusp_location() } -> std::convertible_to<real_t>;
};

// This is the beginning of the real jet. Once it is complete, we will pull it
// out of this namespace and replace current usage.

namespace math {

using std::abs;
using std::copysign;
using std::exp;
using std::hypot;
using std::isfinite;
using std::isnan;
using std::log;
using std::log1p;
using std::pow;
using std::sqrt;
using std::tanh;

// ----------------------------------------------------------------------------
// Scalar Fallbacks
// ----------------------------------------------------------------------------

template <typename Element>
constexpr auto primal(const Element& x) noexcept -> Element {
  return x;
}

template <typename Element>
constexpr auto derivative(const Element&) noexcept -> Element {
  return Element{0};
}

// ----------------------------------------------------------------------------
// Jet
// ----------------------------------------------------------------------------

/*!
  A jet is an algebraic structure used to perform forward-mode automatic
  differentiation. It generalizes a scalar value to carry its own derivative,
  allowing simultaneous evaluation of a function and its gradient.

  This implementation represents 1-jets using algebraic dual numbers of the
  form `a + vε`, where `ε^2 = 0`, but `ε != 0`. With this property, arithmetic
  operations on jets behave like a truncated Taylor series expansion,
  effectively encoding derivatives into a polynomial. When a function is
  applied to a jet, the real part represents the function's value and the dual
  part, the coefficient of `ε`, contains the first derivative.

  These are very similar to complex numbers, but instead of representing
  rotations, they represent the chain rule at machine precision.

  \par Composability
  The jet algebra is closed, allowing jets to be nested. Instantiating
  a jet<jet<T>> introduces a second, distinct infinitesimal unit, and the
  composition represents the original jet and its 2nd derivative.
*/
template <typename Element = real_t>
struct Jet {
  Element a{0};
  Element v{0};

  // --------------------------------------------------------------------------
  // Construction
  // --------------------------------------------------------------------------

  Jet() = default;
  constexpr Jet(const Element& a, const Element& v) noexcept : a{a}, v{v} {}
  constexpr Jet(const Element& s) noexcept : a{s}, v{0} {}

  // --------------------------------------------------------------------------
  // Conversions
  // --------------------------------------------------------------------------

  template <typename OtherElement>
  explicit constexpr Jet(const Jet<OtherElement>& other) noexcept
      : a{static_cast<Element>(other.a)}, v{static_cast<Element>(other.v)} {}

  template <typename OtherElement>
  constexpr auto operator=(const Jet<OtherElement>& rhs) noexcept -> Jet& {
    a = static_cast<Element>(rhs.a);
    v = static_cast<Element>(rhs.v);
    return *this;
  }

  explicit constexpr operator bool() const noexcept {
    // Ignore derivative.
    return a != Element{0};
  }

  // --------------------------------------------------------------------------
  // Accessors
  // --------------------------------------------------------------------------

  friend constexpr auto primal(const Jet& x) noexcept -> Element { return x.a; }
  friend constexpr auto derivative(const Jet& x) noexcept -> Element {
    return x.v;
  }

  // --------------------------------------------------------------------------
  // Unary Arithmetic
  // --------------------------------------------------------------------------

  friend constexpr auto operator+(const Jet& x) noexcept -> Jet { return x; }
  friend constexpr auto operator-(const Jet& x) noexcept -> Jet {
    return {-x.a, -x.v};
  }

  // --------------------------------------------------------------------------
  // Element Arithmetic
  // --------------------------------------------------------------------------

  constexpr auto operator*=(const Element& x) noexcept -> Jet& {
    a *= x;
    v *= x;
    return *this;
  }

  constexpr auto operator/=(const Element& x) noexcept -> Jet& {
    const auto inv = 1.0 / x;
    a *= inv;
    v *= inv;
    return *this;
  }

  friend constexpr auto operator*(Jet lhs, const Element& rhs) noexcept -> Jet {
    return lhs *= rhs;
  }

  friend constexpr auto operator*(const Element& lhs, Jet rhs) noexcept -> Jet {
    // Commute.
    return rhs *= lhs;
  }

  friend constexpr auto operator/(Jet lhs, const Element& rhs) noexcept -> Jet {
    return lhs /= rhs;
  }

  // --------------------------------------------------------------------------
  // Vector Arithmetic
  // --------------------------------------------------------------------------

  constexpr auto operator+=(const Jet& x) noexcept -> Jet& {
    a += x.a;
    v += x.v;
    return *this;
  }

  constexpr auto operator-=(const Jet& x) noexcept -> Jet& {
    a -= x.a;
    v -= x.v;
    return *this;
  }

  // d(xy) = x*dy + dx*y, product rule
  constexpr auto operator*=(const Jet& x) noexcept -> Jet& {
    // product rule, (uv)' = uv' + u'v:
    v = a * x.v + v * x.a;
    a *= x.a;
    return *this;
  }

  // d(u/v) = (du*v - u*dv)/v^2 = (du - u*dv/v)/v, quotient rule
  constexpr auto operator/=(const Jet& x) noexcept -> Jet& {
    // This should look suspicious intially, because we modify a then use it
    // to calc v, but it's actually a deliberate optimization.
    const auto inv = 1.0 / x.a;
    a *= inv;
    v = (v - a * x.v) * inv;
    return *this;
  }

  friend constexpr auto operator+(Jet lhs, const Jet& rhs) noexcept -> Jet {
    return lhs += rhs;
  }

  friend constexpr auto operator-(Jet lhs, const Jet& rhs) noexcept -> Jet {
    return lhs -= rhs;
  }

  friend constexpr auto operator*(Jet lhs, const Jet& rhs) noexcept -> Jet {
    return lhs *= rhs;
  }

  friend constexpr auto operator/(Jet lhs, const Jet& rhs) noexcept -> Jet {
    return lhs /= rhs;
  }

  // --------------------------------------------------------------------------
  // Element Comparison
  // --------------------------------------------------------------------------

  friend constexpr auto operator<=>(const Jet& lhs, const Element& rhs) noexcept
      -> std::common_comparison_category_t<decltype(lhs.a <=> rhs),
                                           std::weak_ordering> {
    return lhs.a <=> rhs;
  }

  friend constexpr auto operator==(const Jet& lhs, const Element& rhs) noexcept
      -> bool {
    return lhs.a == rhs && lhs.v == Element{0};
  }

  // --------------------------------------------------------------------------
  // Vector Comparison
  // --------------------------------------------------------------------------

  // Jet ignores the derivative for ordering, so impose a weak ordering at best.
  constexpr auto operator<=>(const Jet& other) const noexcept
      -> std::common_comparison_category_t<decltype(a <=> other.a),
                                           std::weak_ordering> {
    return a <=> other.a;
  }

  constexpr auto operator==(const Jet&) const noexcept -> bool = default;

  // --------------------------------------------------------------------------
  // Selection
  // --------------------------------------------------------------------------

  // d(min(x, y)) = dx if x < y else dy
  friend constexpr auto min(const Jet& x, const Jet& y) noexcept -> Jet {
    return x.a < y.a ? x : y;
  }

  // d(max(x, y)) = dx if x > y else dy
  friend constexpr auto max(const Jet& x, const Jet& y) noexcept -> Jet {
    return x.a > y.a ? x : y;
  }

  // d(clamp(x, lo, hi)) = 0 if clamped, dv otherwise
  friend constexpr auto clamp(const Jet& x, const Jet& lo,
                              const Jet& hi) noexcept -> Jet {
    if (x.a < lo.a) return lo;
    if (x.a > hi.a) return hi;
    return x;
  }

  // --------------------------------------------------------------------------
  // Classification
  // --------------------------------------------------------------------------

  friend auto isfinite(const Jet& x) noexcept -> bool {
    using math::isfinite;
    return isfinite(x.a) && isfinite(x.v);
  }

  friend auto isnan(const Jet& x) noexcept -> bool {
    using math::isnan;
    return isnan(x.a) || isnan(x.v);
  }

  // --------------------------------------------------------------------------
  // Math Functions
  // --------------------------------------------------------------------------

  // d(abs(x)) = sgn(x)*dx
  friend constexpr auto abs(const Jet& x) noexcept -> Jet {
    using math::abs;

    return {abs(x.a), copysign(Element{1}, x.a) * x.v};
  }

  //! \pre sgn != 0
  // d(copysign(x, y)) = sgn(x)sgn(y)*dx
  friend constexpr auto copysign(const Jet& mag, const Jet& sgn) noexcept
      -> Jet {
    using math::copysign;

    const auto dirac_delta = (sgn.a == Element{0})
                                 ? std::numeric_limits<Element>::infinity()
                                 : Element{0};

    const auto sgn_mag = copysign(Element{1}, mag.a);
    const auto sgn_sgn = copysign(Element{1}, sgn.a);

    // Product rule for |x| * sgn(y):
    // d(|x|) * sgn(y) + |x| * d(sgn(y))
    //  = (sgn(x)*dx) * sgn(y) + |x| * (delta(y)*dy)
    const auto v_mag = sgn_mag * sgn_sgn * mag.v;
    const auto v_sgn = abs(mag.a) * dirac_delta * sgn.v;

    return {copysign(mag.a, sgn.a), v_mag + v_sgn};
  }

  // d(exp(x)) = exp(x)*dx
  friend constexpr auto exp(const Jet& x) noexcept -> Jet {
    using math::exp;
    const auto exp_a = exp(x.a);
    return {exp_a, exp_a * x.v};
  }

  // d(hypot(x, y)) = (x*dx + y*dy) / hypot(x, y)
  friend auto hypot(const Jet& x, const Jet& y) noexcept -> Jet {
    using math::hypot;

    const auto mag = hypot(x.a, y.a);
    if (mag == Element{0}) {
      return {Element{0}, Element{0}};
    }

    return {mag, (x.a * x.v + y.a * y.v) / mag};
  }

  //! \pre x > 0
  // d(log(x)) = dx/x
  friend constexpr auto log(const Jet& x) noexcept -> Jet {
    using math::log;

    assert(x.a > Element{0} && "Jet::log domain error");

    return {log(x.a), x.v / x.a};
  }

  //! \pre x > -1
  // d(log1p(x)) = dx/(x + 1)
  friend constexpr auto log1p(const Jet& x) noexcept -> Jet {
    using math::log1p;

    assert(x.a > Element{-1} && "Jet::log1p domain error");

    return {log1p(x.a), x.v / (x.a + 1)};
  }

  //! \pre x > 0 || (x == 0 && y >= 1)
  // jet^element
  // d(x^y) = y*x^(y - 1)*dx
  friend constexpr auto pow(const Jet& x, const Element& y) noexcept -> Jet {
    using math::pow;

    // We restrict the range to positive numbers or 0 with a positive exponent.
    //
    // x < 0:
    // The vast majority of the domain has nonreal results and we don't support
    // complex jets. The only real results come from negative integers, which
    // don't come up in our usage. Instead of bothering with an int check, all
    // of x < 0 is excluded.
    //
    // x == 0:
    // The result is Inf if y < 1.
    assert((x > 0 || (x == 0 && y >= 1)) &&
           "Jet::pow(<jet>, <element>) domain error");

    const auto pm1 = pow(x.a, y - Element(1));
    return {pm1 * x.a, y * pm1 * x.v};
  }

  //! \pre x > 0
  // element^jet
  // d(b^y) = ln(b)*b^y*dy
  friend constexpr auto pow(const Element& base, const Jet& y) noexcept -> Jet {
    using math::pow;

    assert(base > Element(0) && "Jet::pow(<element>, <jet>) domain error");

    const auto power = pow(base, y.a);
    const auto log_base = log(base);

    return {power, log_base * power * y.v};
  }

  //! \pre x > 0
  // jet^jet
  // d(x^y) = x^y*(ln(x)*dy + y*dx/x) = (x^y*ln(x)*dy + x^(y - 1)*y*dx)
  friend constexpr auto pow(const Jet& x, const Jet& y) noexcept -> Jet {
    using math::pow;

    assert(x.a > Element{0} && "Jet::pow(<jet>, <jet>) domain error");

    /*
      By definition:

        x^y = e^(ln(x)*y)
        d(e^(f(x))) = e^(f(x))d(f(x))

      Here, f(x) = ln(x)*y:

        d(f(x)) = ln(x)*d(y) + d(ln(x))*y
                = ln(x)*dy + y*dx/x

      Using this, the full derivation is:

        d(x^y) = e^(ln(x)*y)(ln(x)*dy + y*dx/x)
               = (x^y)(ln(x)*dy + y*dx/x)
               = x^y*ln(x)*dy + x^(y - 1)*y*dx

      The familiar power rule is recovered when y is a constant because that
      makes dy = 0.
    */
    const auto pm1 = pow(x.a, y.a - 1);
    const auto power = x.a * pm1;
    return {power, power * log(x.a) * y.v + pm1 * y.a * x.v};
  }

  // d(sqrt(x)) = dx / (2 * sqrt(x))
  friend constexpr auto sqrt(const Jet& x) noexcept -> Jet {
    using math::sqrt;

    assert(x.a >= Element{0} && "Jet::sqrt domain error");

    const auto root = sqrt(x.a);
    if (root == Element{0}) {
      return {Element{0}, std::numeric_limits<Element>::infinity()};
    }

    return {root, x.v / (Element{2} * root)};
  }

  // d(tanh(x)) = (1 - tanh(x)^2)*dx
  friend constexpr auto tanh(const Jet& x) noexcept -> Jet {
    using math::tanh;
    const auto tanh_a = tanh(x.a);
    return {tanh_a, (Element{1} - tanh_a * tanh_a) * x.v};
  }

  // --------------------------------------------------------------------------
  // Standard Library Integration
  // --------------------------------------------------------------------------

  friend auto operator<<(std::ostream& dst, const Jet& src) -> std::ostream& {
    return dst << "{.a = " << src.a << ", .v = " << src.v << "}";
  }

  friend constexpr auto swap(Jet& lhs, Jet& rhs) noexcept -> void {
    using std::swap;
    swap(lhs.a, rhs.a);
    swap(lhs.v, rhs.v);
  }
};

}  // namespace math
}  // namespace curves

namespace std {

template <typename Element>
struct numeric_limits<curves::math::Jet<Element>> : numeric_limits<Element> {
  static constexpr bool is_specialized = true;

  using Base = numeric_limits<Element>;

  static constexpr auto min() noexcept -> curves::math::Jet<Element> {
    return Base::min();
  }

  static constexpr auto max() noexcept -> curves::math::Jet<Element> {
    return Base::max();
  }

  static constexpr auto lowest() noexcept -> curves::math::Jet<Element> {
    return Base::lowest();
  }

  static constexpr auto epsilon() noexcept -> curves::math::Jet<Element> {
    return Base::epsilon();
  }

  static constexpr auto round_error() noexcept -> curves::math::Jet<Element> {
    return Base::round_error();
  }

  static constexpr auto infinity() noexcept -> curves::math::Jet<Element> {
    return Base::infinity();
  }

  static constexpr auto quiet_NaN() noexcept -> curves::math::Jet<Element> {
    return Base::quiet_NaN();
  }

  static constexpr auto signaling_NaN() noexcept -> curves::math::Jet<Element> {
    return Base::signaling_NaN();
  }

  static constexpr auto denorm_min() noexcept -> curves::math::Jet<Element> {
    return Base::denorm_min();
  }
};

}  // namespace std
