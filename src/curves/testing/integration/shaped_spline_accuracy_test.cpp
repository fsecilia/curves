// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include <curves/testing/test.hpp>
#include <curves/curves/synchronous.hpp>
#include <curves/math/io.hpp>
#include <curves/math/shaped_spline_builder.hpp>
#include <curves/math/shaped_spline_view.hpp>
#include <curves/testing/error_metrics.hpp>
#include <curves/testing/shaped_spline_accuracy.hpp>

namespace curves {
namespace {

struct ShapedSplineAccuracyTest : Test {
  AccuracyMetrics accuracy_metrics;

  SynchronousCurve curve{8.0L, 0.5L, 10.55L, 0.5L};
};

// ============================================================================
// Test Harness
// ============================================================================

template <GeneratingCurve Curve>
void run_accuracy_test(const Curve& curve, const InputShapingView& shaping,
                       CurveInterpretation interp,
                       const ShapedSplineConfig& config,
                       const std::string& test_name) {
  std::cout.precision(20);

  std::cout << "=== " << test_name << " ===" << std::endl;

  // Build the spline
  auto spline = build_shaped_spline(curve, shaping, interp, config);

  std::cout << "Segments: " << spline.num_segments << std::endl;

  // Measure accuracy with appropriate oracle
  AccuracyResult result;

  if (interp == CurveInterpretation::kSensitivity) {
    auto oracle = SensitivityShapedEvaluator{curve, shaping};
    result = measure_accuracy(spline, oracle, config.floor.sensitivity_offset,
                              config.subdivision.v_max);
  } else {
    auto oracle = GainShapedEvaluator{curve, shaping, config.subdivision.v_max};

    std::cout << "Cache check at key points:\n";
    for (real_t v : {0.0L, 2.0L, 3.0L, 50.0L, 100.0L}) {
      auto [T, dT] = oracle(v);
      real_t u = shaping.eval(v);
      std::cout << "  v=" << v << " u=" << u << " T=" << T << " dT=" << dT
                << "\n";
    }

    ShapedSplineView view{&spline};
    auto [T, dT, d2T] = view(128.0);
    std::cout << "View at v=128: T=" << T << " dT=" << dT << "\n";

    // Compare to evaluator
    auto [T_eval, dT_eval] = oracle(128.0);
    std::cout << "Eval at v=128: T=" << T_eval << " dT=" << dT_eval << "\n";

    result = measure_accuracy(spline, oracle, config.floor.sensitivity_offset,
                              config.subdivision.v_max);
  }

  std::cout << "Max T error:  " << result.max_T_error
            << " at v=" << result.max_T_error_at_v << std::endl;
  std::cout << "Max T' error: " << result.max_dT_error
            << " at v=" << result.max_dT_error_at_v << std::endl;
  std::cout << "RMS T error:  " << result.rms_T_error << std::endl;
  std::cout << "RMS T' error: " << result.rms_dT_error << std::endl;
  std::cout << std::endl;
}

// ============================================================================
// Tests
// ============================================================================

TEST(shaped_spline_accuracy, power_law_sensitivity) {
  PowerLaw curve{.gamma = 0.5, .scale = 2.0};

  curves_shaping_params shaping_params = solve_input_shaping(
      InputShapingConfig{.floor_v_width = 2.0, .ease_in_width = 0.1});
  InputShapingView shaping{&shaping_params};

  ShapedSplineConfig config{
      .floor = {.v_width = shaping.ease_in_transition_v_begin(),
                .sensitivity_offset = 1.0},
      .subdivision = {.tolerance = 1e-4, .v_max = 128.0},
  };

  run_accuracy_test(curve, shaping, CurveInterpretation::kSensitivity, config,
                    "PowerLaw gamma=0.5 as Sensitivity");
}

TEST(shaped_spline_accuracy, power_law_gain) {
  PowerLaw curve{.gamma = 1.0, .scale = 1.5};

  curves_shaping_params shaping_params =
      solve_input_shaping(InputShapingConfig{.floor_v_width = 2.0});
  InputShapingView shaping{&shaping_params};

  ShapedSplineConfig config{
      .floor = {.v_width = shaping.ease_in_transition_v_begin(),
                .sensitivity_offset = 0.5},
      .subdivision = {.tolerance = 1e-4, .v_max = 128.0},
  };

  run_accuracy_test(curve, shaping, CurveInterpretation::kGain, config,
                    "PowerLaw gamma=1.0 as Gain");
}

TEST(shaped_spline_accuracy, log1p_sensitivity) {
  Log1p curve{.scale = 3.0, .rate = 0.1};

  curves_shaping_params shaping_params =
      solve_input_shaping(InputShapingConfig{.floor_v_width = 1.0});
  InputShapingView shaping{&shaping_params};

  ShapedSplineConfig config{
      .floor = {.v_width = shaping.ease_in_transition_v_begin(),
                .sensitivity_offset = 0.0},
      .subdivision = {.tolerance = 1e-5, .v_max = 128.0},
  };

  run_accuracy_test(curve, shaping, CurveInterpretation::kSensitivity, config,
                    "Log1p as Sensitivity");
}

}  // namespace
}  // namespace curves
