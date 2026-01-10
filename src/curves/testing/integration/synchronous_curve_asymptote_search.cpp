// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include <curves/testing/test.hpp>
#include <curves/math/curves/synchronous.hpp>

namespace curves {
namespace {

/*
  We were kicking around the possibility that if we clamped smoothness to
  (0, 0.5], we might be able to use richardson extrapolation over corrected
  trapezoid to integrate this when it is interpreted as gain. This test shows
  there are many valid parameterizations that cause the first derivative to
  explode at 0, so gaussian we went.

  This isn't so much an integration test as it is a long-running search. It
  should go in its own executable, but it would be the only one so far. It's
  unlikley to be used again, but it's worth keeping around for posterity. If we
  do end up having other investigatory executables, we'll move this to one.
  For now, only enable it conditionally.
*/

// #define enable_asymptote_search
#if defined enable_asymptote_search

struct DerivativeBehavior {
  double max_derivative;
  double x_at_max;
  bool has_numerical_issues;
};

DerivativeBehavior analyze_curve(double motivity, double gamma,
                                 double sync_speed, double smooth) {
  SynchronousCurve curve(motivity, gamma, sync_speed, smooth);

  // Sample points in log-space to catch behavior near 0 and p
  std::vector<double> test_points;

  // Near 0
  for (int i = -15; i < 0; ++i) test_points.push_back(std::pow(10.0, i));

  // Linear sampling in [1e-5, 10*sync_speed]
  double step = sync_speed / 1000;
  for (double x = 1e-5; x < 10 * sync_speed; x += step)
    test_points.push_back(x);

  // Dense sampling around the cusp
  for (double delta = -0.01; delta <= 0.01; delta += 0.0001)
    if (sync_speed + delta > 0) test_points.push_back(sync_speed + delta);

  DerivativeBehavior result{0, 0, false};

  for (double x : test_points) {
    auto [f, df] = curve(x);

    if (std::isnan(df) || std::isinf(df)) {
      result.has_numerical_issues = true;
      continue;
    }

    if (std::abs(df) > result.max_derivative) {
      result.max_derivative = std::abs(df);
      result.x_at_max = x;
    }
  }

  return result;
}

TEST(SynchronousCurve, asymptote_search) {
  // Grid over parameter space
  std::vector<double> motivities = {1.01, 1.1, 1.5, 2, 5, 10, 100, 1000};
  std::vector<double> gammas = {0.01, 0.1, 0.5, 1, 2, 5, 10, 100};
  std::vector<double> sync_speeds = {0.01, 0.1, 1, 5, 10, 100};
  std::vector<double> smooths = {0.01, 0.1, 0.25, 0.5};  // clamped to ≤ 0.5

  double threshold = 1e10;  // what counts as "exploding"

  for (auto m : motivities)
    for (auto g : gammas)
      for (auto p : sync_speeds)
        for (auto s : smooths) {
          auto result = analyze_curve(m, g, p, s);
          if (result.max_derivative > threshold ||
              result.has_numerical_issues) {
            std::cout << "Issue at: motivity=" << m << " gamma=" << g
                      << " sync_speed=" << p << " smooth=" << s
                      << " max_deriv=" << result.max_derivative
                      << " at x=" << result.x_at_max << "\n";
          }
        }
}
#endif

}  // namespace
}  // namespace curves
