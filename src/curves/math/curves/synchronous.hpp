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
#include <array>
#include <cmath>

namespace curves {

template <typename Scalar>
class Synchronous {
 public:
  Synchronous() noexcept
      : Synchronous{Scalar{1.5}, Scalar{1}, Scalar{5.0}, Scalar{0.5}} {}

  Synchronous(Scalar m, Scalar g, Scalar p, Scalar k) noexcept
      : m_{m},
        l_{math::log(m)},
        g_{g / l_},
        p_{p},
        k_{math::min(k == Scalar{0} ? Scalar{32} : Scalar{0.5} / k,
                     Scalar{32})},
        r_{Scalar{1} / k_} {}

  template <typename Value>
  auto operator()(Value x) const noexcept -> Value {
    using namespace math;

    // Use limit definition near 0.
    if (x < std::numeric_limits<Value>::epsilon()) {
      return Value{1} / m_ + Value{0} * x;
    }

    // Use linear taylor approximation (very) near to cusp.
    const auto displacement = x - p_;
    if (abs(primal(displacement)) <= kCuspApproximationDistance) {
      return Value{1} + (l_ * g_ / p_) * displacement;
    }

    const auto u = g_ * log(x / p_);
    const auto w = tanh(pow(abs(u), k_));
    return exp(copysign(l_, u) * pow(w, r_));
  }

  auto critical_points() const noexcept -> std::array<Scalar, 1> {
    return {p_};
  }

 private:
  static constexpr auto kCuspApproximationDistance = Scalar{1e-7};

  real_t m_;
  real_t l_;
  real_t g_;
  real_t p_;
  real_t k_;
  real_t r_;
};

/*
  This is the original version that doesn't work with the new, real jets.
  It's sticking around for a bit until we're ready to make the transition.
*/
class SynchronousCurve {
 public:
  SynchronousCurve() noexcept : SynchronousCurve(1.5, 1.0, 5.0, 0.5) {}

  SynchronousCurve(real_t motivity, real_t gamma, real_t sync_speed,
                   real_t smooth) noexcept
      : motivity_{motivity},
        L_{std::log(motivity)},
        g_{gamma / L_},
        p_{sync_speed},
        k_{smooth == 0.0 ? 64.0 : 0.5 / smooth},
        r_{1.0 / k_} {}

  auto cusp_location() const noexcept -> real_t { return p_; }

  auto value(real_t x) const noexcept -> real_t {
    // Use limit definition near 0.
    if (x < std::numeric_limits<real_t>::epsilon()) {
      return 1.0 / motivity_;
    }

    // Use linear taylor approximation (very) near to cusp.
    const auto displacement = x - p_;
    if (std::abs(displacement) <= kCuspApproximationDistance) {
      return 1.0 + (L_ * g_ / p_) * displacement;
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
      return {1.0 + slope * displacement, slope};
    }

    const auto u = g_ * std::log(x / p_);
    const auto sign = std::copysign(1.0, u);
    const auto u_abs = std::abs(u);

    const auto u_km1 = std::pow(u_abs, k_ - 1);
    const auto u_k = u_km1 * u_abs;
    const auto w = std::tanh(u_k);
    const auto w_rm1 = std::pow(w, r_ - 1);
    const auto w_r = w_rm1 * w;

    const auto f = std::exp(sign * L_ * w_r);
    const auto sech2 = 1.0 - w * w;
    const auto fp = (f * L_ * g_ / x) * u_km1 * w_rm1 * sech2;

    return {f, fp};
  }

 private:
  static constexpr auto kCuspApproximationDistance = 1e-7;

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
  Param<double> smooth{"Smooth", 0.5, 1.0 / 32, 1.0};
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
    return SynchronousCurve{
        static_cast<real_t>(motivity.value()),
        static_cast<real_t>(gamma.value()),
        static_cast<real_t>(sync_speed.value()),
        static_cast<real_t>(smooth.value()),
    };
  }
};

}  // namespace curves
