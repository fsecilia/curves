// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Transfer function adapter and related traits.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/math/curve.hpp>
#include <limits>
#include <utility>

namespace curves {

template <typename Traits, typename Curve>
concept HasAntiderivative =
    requires(const Traits& t, const Curve& c, real_t x) {
      { t.antiderivative(c, x) } -> std::convertible_to<real_t>;
    };

template <typename Curve>
struct TransferFunctionTraits {
  auto eval_at_0(const Curve& curve) const noexcept -> Jet {
    return {0, curve(0.0).f};
  }
};

template <typename Curve, typename Traits = TransferFunctionTraits<Curve>>
class TransferFunction {
 public:
  auto operator()(real_t x) const noexcept -> Jet {
    if (x < std::numeric_limits<real_t>::epsilon()) {
      return traits_.eval_at_0(curve_);
    }

    const auto curve_result = curve_(x);
    return {x * curve_result.f, curve_result.f + x * curve_result.df};
  }

  auto cusp_location() const noexcept -> real_t
    requires HasCusp<Curve>
  {
    return curve_.cusp_location();
  }

  explicit TransferFunction(Curve curve, Traits traits = {}) noexcept
      : curve_{std::move(curve)}, traits_{std::move(traits)} {}

 private:
  Curve curve_;
  Traits traits_;
};

}  // namespace curves
