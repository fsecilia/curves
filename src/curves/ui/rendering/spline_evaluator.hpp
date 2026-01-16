// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Floating point evaluator for S(x), S'(x), G(x), and G'(x), given T(x).

  This class takes the transfer function approximation and synthesizes the
  4 curves to show in the ui.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/math/fixed.hpp>
#include <curves/math/spline.hpp>
#include <curves/ui/rendering/spline_sampler.hpp>

namespace curves {

struct CurveValues {
  real_t sensitivity;
  real_t sensitivity_deriv;
  real_t gain;
  real_t gain_deriv;
};

class CurveEvaluator {
 public:
  auto compute(const SplineSample& sample, real_t x_logical) -> CurveValues {
    // P'(t) = 3at^2 + 2bt + c
    const auto p_prime =
        (3.0 * sample.a * sample.t + 2.0 * sample.b) * sample.t + sample.c;
    // P''(t) = 6at + 2b
    const auto p_double_prime = 6.0 * sample.a * sample.t + 2.0 * sample.b;

    const auto gain = p_prime * sample.inv_width;

    // inv_width_sq = inv_width * inv_width
    const auto gain_deriv =
        p_double_prime * (sample.inv_width * sample.inv_width);

    auto sens = 0.0;
    auto sens_deriv = 0.0;

    // Sensitivity
    if (sample.is_start_segment) {
      // S(t) = (at^2 + bt + c) * inv_width
      const auto s_poly =
          (sample.a * sample.t + sample.b) * sample.t + sample.c;
      sens = s_poly * sample.inv_width;

      // S'(t) = (2at + b) * inv_width^2
      const auto s_prime_poly = 2.0 * sample.a * sample.t + sample.b;
      sens_deriv = s_prime_poly * (sample.inv_width * sample.inv_width);

    } else {
      // T(t) = ((at + b)t + c)t + d
      const auto transfer =
          ((sample.a * sample.t + sample.b) * sample.t + sample.c) * sample.t +
          sample.d;

      sens = transfer / x_logical;
      sens_deriv = (gain - sens) / x_logical;
    }

    return CurveValues{sens, sens_deriv, gain, gain_deriv};
  }
};

}  // namespace curves
