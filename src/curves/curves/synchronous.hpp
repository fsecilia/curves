// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Synchronous curve.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/config/curve.hpp>
#include <curves/config/param.hpp>
#include <curves/math/curve.hpp>
#include <curves/math/transfer_function.hpp>
#include <cmath>

namespace curves {

class SynchronousCurve {
 public:
  SynchronousCurve() noexcept : SynchronousCurve(1.0, 1.5, 1.0, 5.0, 0.5) {}

  SynchronousCurve(real_t scale, real_t motivity, real_t gamma,
                   real_t sync_speed, real_t smooth) noexcept
      : scale_{scale},
        motivity_{motivity},
        L{std::log(motivity)},
        g{gamma / L},
        p{sync_speed},
        k{smooth == 0 ? 64.0 : 0.5 / smooth},
        r{1.0 / k} {}

  auto scale() const noexcept -> real_t { return scale_; }
  auto motivity() const noexcept -> real_t { return motivity_; }

  auto operator()(real_t x) const noexcept -> CurveResult {
    if (std::abs(x - p) <= std::numeric_limits<real_t>::epsilon()) {
      return {scale_, scale_ * L * g / p};
    }

    if (x > p) {
      return evaluate<+1>(g * (std::log(x) - std::log(p)), x);
    } else {
      return evaluate<-1>(g * (std::log(p) - std::log(x)), x);
    }
  }

 private:
  real_t scale_;
  real_t motivity_;

  real_t L;  // log(motivity)
  real_t g;  // gamma / L
  real_t p;  // sync_speed
  real_t k;  // sharpness = 0.5 / smooth
  real_t r;  // 1 / sharpness

  // 'sign' is +1 for x > p, -1 for x < p.
  // It only affects the exponent of f; the derivative formula is invariant.
  template <int sign>
  auto evaluate(real_t u, real_t x) const noexcept -> CurveResult {
    // Shared intermediate terms
    real_t u_pow_k_minus_1 = std::pow(u, k - 1);
    real_t u_pow_k = u_pow_k_minus_1 * u;  // v = u^k

    real_t w = std::tanh(u_pow_k);  // w = tanh(v)
    real_t w_pow_r_minus_1 = std::pow(w, r - 1);
    real_t w_pow_r = w_pow_r_minus_1 * w;  // z = w^r

    real_t sech_sq = 1 - w * w;  // sech(v)^2

    // Forward: f = exp((+/-)L * z)
    real_t f = scale_ * std::exp(sign * L * w_pow_r);

    // Derivative: df/dx = (f * L * g / x) * u^(k-1) * w^(r-1) * sech(v)^2
    real_t df_dx =
        (f * L * g / x) * u_pow_k_minus_1 * w_pow_r_minus_1 * sech_sq;

    return {f, df_dx};
  }
};

struct SynchronousCurveConfig {
  Param<double> scale{"scale", 1.0, 1.0e-3, 1.0e3};
  Param<double> motivity{"motivity", 1.5, 1.0, 1.0e3};
  Param<double> gamma{"gamma", 1, 1.0, 1.0e3};
  Param<double> smooth{"smooth", 0.5, 0.0, 1.0};
  Param<double> sync_speed{"sync_speed", 5, 1.0e-3, 1.0e3};

  auto reflect(this auto&& self, auto&& visitor) -> void {
    self.scale.reflect(visitor);
    self.motivity.reflect(visitor);
    self.gamma.reflect(visitor);
    self.smooth.reflect(visitor);
    self.sync_speed.reflect(visitor);
  }

  auto validate(auto&& visitor) -> void {
    scale.validate(visitor);
    motivity.validate(visitor);
    gamma.validate(visitor);
    smooth.validate(visitor);
    sync_speed.validate(visitor);
  }

  auto create() const -> SynchronousCurve {
    return SynchronousCurve{scale.value(), motivity.value(), gamma.value(),
                            sync_speed.value(), smooth.value()};
  }
};

template <>
struct TransferFunctionTraits<SynchronousCurve> {
  auto eval_at_0(const SynchronousCurve& curve) const noexcept -> CurveResult {
    /*
      This comes from the limit definition of the derivative of the transfer
      function.
    */
    return {0.0, curve.scale() / curve.motivity()};
  }
};

}  // namespace curves
