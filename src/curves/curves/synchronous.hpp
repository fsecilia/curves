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
#include <curves/math/jet.hpp>
#include <cmath>

namespace curves {

class SynchronousCurve {
 public:
  SynchronousCurve() noexcept : SynchronousCurve(1.5, 1.0L, 5.0L, 0.5L) {}

  SynchronousCurve(real_t motivity, real_t gamma, real_t sync_speed,
                   real_t smooth) noexcept
      : motivity_{motivity},
        L_{std::log(motivity)},
        g_{gamma / L_},
        p_{sync_speed},
        k_{smooth == 0 ? 64.0L : 0.5L / smooth},
        r_{1.0L / k_} {}

  auto cusp_location() const noexcept -> real_t { return p_; }

  auto value(real_t x) const noexcept -> real_t {
    // Use limit definition near 0.
    if (x < std::numeric_limits<real_t>::epsilon()) {
      return 1.0L / motivity_;
    }

    // Use linear taylor approximation (very) near to cusp.
    const auto displacement = x - p_;
    if (std::abs(displacement) <= kCuspApproximationDistance) {
      return 1.0L + (L_ * g_ / p_) * displacement;
    }

    const auto u = g_ * std::log(x / p_);
    const auto w = std::tanh(std::pow(std::abs(u), k_));
    return std::exp(std::copysign(L_, u) * std::pow(w, r_));
  }

  auto operator()(real_t x) const noexcept -> Jet {
    // Use linear taylor approximation (very) near to cusp.
    const auto displacement = x - p_;
    if (std::abs(displacement) <= kCuspApproximationDistance) {
      const auto slope = L_ * g_ / p_;
      return {1.0L + slope * displacement, slope};
    }

    const auto u = g_ * std::log(x / p_);
    const auto sign = std::copysign(1.0L, u);
    const auto u_abs = std::abs(u);

    const auto u_km1 = std::pow(u_abs, k_ - 1);
    const auto u_k = u_km1 * u_abs;
    const auto w = std::tanh(u_k);
    const auto w_rm1 = std::pow(w, r_ - 1);
    const auto w_r = w_rm1 * w;

    const auto f = std::exp(sign * L_ * w_r);
    const auto sech2 = 1.0L - w * w;
    const auto fp = (f * L_ * g_ / x) * u_km1 * w_rm1 * sech2;

    return {f, fp};
  }

 private:
  static constexpr auto kCuspApproximationDistance = 1e-7L;

  real_t motivity_;
  real_t L_;
  real_t g_;
  real_t p_;
  real_t k_;
  real_t r_;
};

struct SynchronousCurveConfig {
  Param<double> motivity{"Motivity", 1.5, 1.0, 1.0e3};
  Param<double> gamma{"Gamma", 1, 1e-3, 1.0e3};
  Param<double> smooth{"Smooth", 0.5, 0.0, 1.0};
  Param<double> sync_speed{"Sync Speed", 5, 1.0e-3, 1.0e3};

  auto reflect(this auto&& self, auto&& visitor) -> void {
    self.motivity.reflect(visitor);
    self.gamma.reflect(visitor);
    self.smooth.reflect(visitor);
    self.sync_speed.reflect(visitor);
  }

  auto validate(auto&& visitor) -> void {
    motivity.validate(visitor);
    gamma.validate(visitor);
    smooth.validate(visitor);
    sync_speed.validate(visitor);
  }

  auto create() const -> SynchronousCurve {
    return SynchronousCurve{motivity.value(), gamma.value(), sync_speed.value(),
                            smooth.value()};
  }
};

}  // namespace curves
