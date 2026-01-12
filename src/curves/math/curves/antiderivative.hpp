// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Efficient antiderivative calculation strategies.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/math/cached_integral.hpp>
#include <curves/math/curves/concepts.hpp>
#include <curves/ranges.hpp>
#include <utility>

namespace curves {

// ----------------------------------------------------------------------------
// Antiderivative
// ----------------------------------------------------------------------------

/*!
  Provides specializations for efficiently evaluating antiderivatives of
  curves.

  When integrating, some curves have closed-form antiderivatives that we can
  evalute directly. Others require numerical integration, which requires a
  cache. This type encapsulates the distinctions, providing specializations for
  both.

  The unspecialized version expects to contain a cached integral function it
  invokes directly.
*/
template <typename Integral>
struct Antiderivative {
  using Scalar = Integral::Scalar;

  Integral integral;

  template <typename Value>
  auto operator()(Value v) const noexcept -> Value {
    return integral(v);
  }
};

/*!
  This specialization expects to contain the function being integrated, not its
  integral. It calls .antiderivative().
*/
template <HasAntiderivative Curve>
struct Antiderivative<Curve> {
  using Scalar = Curve::Scalar;

  Curve curve;

  auto operator()(Scalar v) const noexcept -> Scalar {
    return curve.antiderivative(v);
  }

  template <typename Element>
  auto operator()(const math::Jet<Element>& v) const noexcept
      -> math::Jet<Element> {
    return {curve.antiderivative(v.a), curve(v.a) * v.v};
  }
};

// ----------------------------------------------------------------------------
// AntiderivativeBuilder
// ----------------------------------------------------------------------------

/*!
  Builds adapters that allow calling integrals on functions that may have
  analytical antiderivatives.
*/
template <typename CachedIntegralBuilder, typename Integrator>
struct AntiderivativeBuilder {
  [[no_unique_address]] CachedIntegralBuilder cached_integral_builder;
  [[no_unique_address]] Integrator integrator;

  // If the input curve has no antiderivative, build a cached integral.
  template <typename Curve>
  auto operator()(Curve curve, Curve::Scalar max, Curve::Scalar tolerance,
                  CompatibleRange<typename Curve::Scalar> auto critical_points)
      const -> auto {
    using Integral = ComposedIntegral<Curve, Integrator>;
    return cached_integral_builder(Integral{std::move(curve), integrator}, max,
                                   tolerance, critical_points);
  }

  // If the input curve has an antiderivative, we use the curve directly.
  template <HasAntiderivative Curve>
  auto operator()(Curve curve, Curve::Scalar, Curve::Scalar,
                  CompatibleRange<typename Curve::Scalar> auto) const noexcept
      -> Antiderivative<Curve> {
    return {std::move(curve)};
  }
};

}  // namespace curves
