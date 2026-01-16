// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include <curves/testing/test.hpp>
#include <curves/config/curve.hpp>
#include <curves/math/cached_integral.hpp>
#include <curves/math/curves/shaping/ease_in.hpp>
#include <curves/math/curves/shaping/ease_out.hpp>
#include <curves/math/curves/shaping/shaped_curve.hpp>
#include <curves/math/curves/shaping/transition.hpp>
#include <curves/math/curves/shaping/transition_functions/reflected.hpp>
#include <curves/math/curves/shaping/transition_functions/smoother_step_integral.hpp>
#include <curves/math/curves/spline/shaped_spline_builder.hpp>
#include <curves/math/curves/synchronous.hpp>
#include <curves/math/curves/transfer_function.hpp>
#include <curves/math/integration.hpp>
#include <curves/math/inverse_function.hpp>
#include <curves/math/io.hpp>
#include <curves/math/jet.hpp>
#include <curves/math/shaped_spline_view.hpp>
#include <curves/testing/error_metrics.hpp>
#include <curves/testing/linear_curve.hpp>
#include <curves/testing/shaped_spline_accuracy.hpp>

namespace curves {
namespace {

struct ShapedSplineAccuracyTest : Test {
  using Integrator = Gauss5;
  using NumericIntegralFactory = ComposedIntegralFactory<Integrator>;
  using TransferFunctionBuilder =
      TransferFunctionBuilder<CachedIntegralBuilder, NumericIntegralFactory>;

  using Inverter = InverseViaPartition;

  using TransitionFunction =
      shaping::transition_functions::SmootherStepIntegral;
  using EaseInTransitionFunction = TransitionFunction;
  using EaseOutTransitionFunction =
      shaping::transition_functions::Reflected<TransitionFunction>;

  using EaseInTransition =
      shaping::Transition<EaseInTransitionFunction, Inverter>;
  using EaseOutTransition =
      shaping::Transition<EaseOutTransitionFunction, Inverter>;

  using EaseIn = shaping::EaseIn<EaseInTransition>;
  using EaseOut = shaping::EaseOut<EaseOutTransition>;

  template <typename Curve>
  using ShapedCurve = shaping::ShapedCurve<Curve, EaseIn, EaseOut>;

  AccuracyMetrics accuracy_metrics;

  static constexpr auto domain_max = 256;
  static constexpr auto dx = 0.1;

  template <typename Curve>
  auto run_accuracy_test(const Curve& curve) -> void {
    auto x = 0.0;
    const auto iter_count = static_cast<int_t>(std::round(domain_max / dx));
    for (auto i = 0; i < iter_count; ++i) {
      std::cout << curve(math::Jet{x, 5.0}).a << std::endl;
      x += dx;
    }
  }
};

TEST_F(ShapedSplineAccuracyTest, linear_curve) {
  const auto generating_curve = Log1p{1.0, 1.0};

  const auto shaped_curve =
      ShapedCurve{generating_curve, EaseIn{EaseInTransition{0, 5, {}, {}}},
                  EaseOut{EaseOutTransition{200, 50, {}, {}}}};

  run_accuracy_test(shaped_curve);
}

#if 0
// ============================================================================
// Test Harness
// ============================================================================

template <typename Curve>
void run_accuracy_test(const Curve& curve, const InputShapingView& shaping,
                       CurveDefinition interp, const ShapedSplineConfig& config,
                       const std::string& test_name) {
  std::cout.precision(20);

  std::cout << "=== " << test_name << " ===" << std::endl;

  // Build the spline
  auto spline = build_shaped_spline(curve, shaping, interp, config);

  std::cout << "Segments: " << spline.num_segments << std::endl;

  // Measure accuracy with appropriate oracle
  AccuracyResult result;

  if (interp == CurveDefinition::kVelocityScale) {
    auto oracle = SensitivityShapedEvaluator{curve, shaping};

    std::cout << "Cache check at key points:\n";
    for (real_t v : {0.0L, 2.0L, 3.0L, 50.0L, 100.0L, 128.0L}) {
      real_t u = shaping.eval(v);
      real_t y = curve.value(v);
      auto [T, dT] = oracle(v);
      std::cout << "  v=" << v << " u=" << u << " y=" << y << " T=" << T
                << " dT=" << dT << "\n";
    }

    result = measure_accuracy(spline, oracle, config.floor.sensitivity_offset,
                              config.subdivision.v_max);
  } else {
    auto oracle = GainShapedEvaluator{curve, shaping, config.subdivision.v_max};

    std::cout << "Cache check at key points:\n";
    for (real_t v : {0.0L, 2.0L, 3.0L, 50.0L, 100.0L, 128.0L}) {
      real_t u = shaping.eval(v);
      real_t y = curve.value(v);
      auto [T, dT] = oracle(v);
      std::cout << "  v=" << v << " u=" << u << " y=" << y << " T=" << T
                << " dT=" << dT << "\n";
    }

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

TEST(shaped_spline_accuracy, power_law_sensitivity_gamma_0_5) {
  PowerLaw curve{.gamma = 0.5L, .scale = 1.0L};

  curves_shaping_params shaping_params = solve_input_shaping(
      InputShapingConfig{.floor_v_width = 2.0L, .ease_in_width = 0.1L});
  InputShapingView shaping{&shaping_params};

  ShapedSplineConfig config{
      .floor = {.v_width = shaping.ease_in_transition_v_begin(),
                .sensitivity_offset = 0.0L},
      .ceiling = {.v_begin = shaping.ease_out_transition_v_begin(),
                  .v_width = shaping.ease_out_transition_v_width()},
      .subdivision = {.tolerance = 1e-6L, .v_max = 128.0L},
  };

  run_accuracy_test(curve, shaping, CurveInterpretation::kSensitivity, config,
                    "PowerLaw gamma=0.5 as Sensitivity");
}

TEST(shaped_spline_accuracy, power_law_sensitivity_gamma_1_0) {
  PowerLaw curve{.gamma = 0.5L, .scale = 1.0L};

  curves_shaping_params shaping_params = solve_input_shaping(
      InputShapingConfig{.floor_v_width = 2.0L, .ease_in_width = 0.1L});
  InputShapingView shaping{&shaping_params};

  ShapedSplineConfig config{
      .floor = {.v_width = shaping.ease_in_transition_v_begin(),
                .sensitivity_offset = 0.0L},
      .ceiling = {.v_begin = shaping.ease_out_transition_v_begin(),
                  .v_width = shaping.ease_out_transition_v_width()},
      .subdivision = {.tolerance = 1e-6L, .v_max = 128.0L},
  };

  run_accuracy_test(curve, InputShapingView{},
                    CurveInterpretation::kSensitivity, config,
                    "PowerLaw gamma=1.0 as Sensitivity");
}

TEST(shaped_spline_accuracy, power_law_gain_gamma_0_5) {
  PowerLaw curve{.gamma = 0.5L, .scale = 1.0L};

  curves_shaping_params shaping_params = solve_input_shaping(
      InputShapingConfig{.floor_v_width = 2.0L, .ease_in_width = 0.1L});
  InputShapingView shaping{&shaping_params};

  ShapedSplineConfig config{
      .floor = {.v_width = shaping.ease_in_transition_v_begin(),
                .sensitivity_offset = 0.0L},
      .ceiling = {.v_begin = shaping.ease_out_transition_v_begin(),
                  .v_width = shaping.ease_out_transition_v_width()},
      .subdivision = {.tolerance = 1e-6L, .v_max = 128.0L},
  };

  run_accuracy_test(curve, shaping, CurveInterpretation::kGain, config,
                    "PowerLaw gamma=0.5 as Gain");
}

TEST(shaped_spline_accuracy, power_law_gain_gamma_1_0) {
  PowerLaw curve{.gamma = 1.0L, .scale = 1.0L};

  curves_shaping_params shaping_params = solve_input_shaping(
      InputShapingConfig{.floor_v_width = 2.0L, .ease_in_width = 0.1L});
  InputShapingView shaping{&shaping_params};

  ShapedSplineConfig config{
      .floor = {.v_width = shaping.ease_in_transition_v_begin(),
                .sensitivity_offset = 0.0L},
      .ceiling = {.v_begin = shaping.ease_out_transition_v_begin(),
                  .v_width = shaping.ease_out_transition_v_width()},
      .subdivision = {.tolerance = 1e-6L, .v_max = 128.0L},
  };

  run_accuracy_test(curve, shaping, CurveInterpretation::kGain, config,
                    "PowerLaw gamma=1.0 as Gain");
}

TEST(shaped_spline_accuracy, log1p_sensitivity) {
  Log1p curve{.scale = 1.0L, .rate = 0.1L};

  curves_shaping_params shaping_params = solve_input_shaping(
      InputShapingConfig{.floor_v_width = 2.0L, .ease_in_width = 0.1L});
  InputShapingView shaping{&shaping_params};

  ShapedSplineConfig config{
      .floor = {.v_width = shaping.ease_in_transition_v_begin(),
                .sensitivity_offset = 0.0L},
      .ceiling = {.v_begin = shaping.ease_out_transition_v_begin(),
                  .v_width = shaping.ease_out_transition_v_width()},
      .subdivision = {.tolerance = 1e-6L, .v_max = 128.0L},
  };

  run_accuracy_test(curve, shaping, CurveInterpretation::kSensitivity, config,
                    "Log1p as Sensitivity");
}

TEST(shaped_spline_accuracy, log1p_gain) {
  Log1p curve{.scale = 1.0L, .rate = 0.1L};

  curves_shaping_params shaping_params = solve_input_shaping(
      InputShapingConfig{.floor_v_width = 2.0L, .ease_in_width = 0.1L});
  InputShapingView shaping{&shaping_params};

  ShapedSplineConfig config{
      .floor = {.v_width = shaping.ease_in_transition_v_begin(),
                .sensitivity_offset = 0.0L},
      .ceiling = {.v_begin = shaping.ease_out_transition_v_begin(),
                  .v_width = shaping.ease_out_transition_v_width()},
      .subdivision = {.tolerance = 1e-6L, .v_max = 128.0L},
  };

  run_accuracy_test(curve, shaping, CurveInterpretation::kGain, config,
                    "Log1p as Gain");
}

#endif

}  // namespace
}  // namespace curves
