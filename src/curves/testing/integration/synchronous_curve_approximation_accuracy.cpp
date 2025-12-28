// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include <curves/testing/test.hpp>
#include <curves/curves/synchronous.hpp>
#include <curves/curves/transfer_function/curve.hpp>
#include <curves/curves/transfer_function/from_gain.hpp>
#include <curves/curves/transfer_function/from_sensitivity.hpp>
#include <curves/math/io.hpp>
#include <curves/math/spline.hpp>
#include <curves/testing/error_metrics.hpp>

namespace curves {
namespace {

TEST(spline_set, synchronous_accuracy_from_sensitivity) {
  auto sensitivity =
      FromSensitivity{SynchronousCurve{8.0L, 0.5L, 10.55L, 0.5L}};

  const auto spline = spline::create_spline(sensitivity, 1.0L);
  const auto v_to_x = Fixed::from_raw(spline.v_to_x);

  std::cout << "spline.v_to_x ~= " << v_to_x.to_real() << " (" << v_to_x.raw
            << " fixed)" << std::endl;

  const auto x_max = Fixed::from_raw(spline.x_geometric_limit);

  const auto dx = Fixed{1.0e-3L};
  std::cout << "dx: " << dx << " (" << dx.raw << " fixed)" << std::endl;

  auto x_fixed = Fixed{0};
  std::cout << "x0: " << x_fixed << " (" << x_fixed.raw << " fixed)"
            << std::endl;

  auto accuracy_metrics = AccuracyMetrics{};
  while (x_fixed < x_max) {
    const auto x_float = x_fixed.to_real();

    const auto y_curve = sensitivity(x_float).f;
    const auto y_table = spline::eval(&spline, x_fixed.raw);
    x_fixed += dx;

    const auto expected = y_curve;
    const auto actual = Fixed::from_raw(y_table).to_real();

    accuracy_metrics.sample(x_float, actual, expected);
  }
  std::cout << "x1: " << x_fixed << " (" << x_fixed.raw << " fixed)"
            << std::endl;

  std::cout << accuracy_metrics << std::endl;

  ASSERT_LE(accuracy_metrics.mse_abs(), 1.1212e-05L);
  ASSERT_LE(accuracy_metrics.rmse_abs(), 2.606065e-06L);
  ASSERT_LE(accuracy_metrics.mse_rel(), 1.22622e-06L);
  ASSERT_LE(accuracy_metrics.rmse_rel(), 9.5155e-09L);
}

TEST(spline_set, synchronous_accuracy_from_gain) {
  auto spline_gain = FromGain{SynchronousCurve{8.0L, 0.5L, 10.55L, 0.5L}};
  auto test_gain = spline_gain;  // make copy

  const auto spline = spline::create_spline(spline_gain, 1.0L);
  const auto v_to_x = Fixed::from_raw(spline.v_to_x);

  std::cout << "spline.v_to_x ~= " << v_to_x.to_real() << " (" << v_to_x.raw
            << " fixed)" << std::endl;

  const auto x_max = Fixed::from_raw(spline.x_geometric_limit);

  const auto dx = Fixed{1.0e-3L};
  std::cout << "dx: " << dx << " (" << dx.raw << " fixed)" << std::endl;

  auto x_fixed = Fixed{0};
  std::cout << "x0: " << x_fixed << " (" << x_fixed.raw << " fixed)"
            << std::endl;

  auto accuracy_metrics = AccuracyMetrics{};
  while (x_fixed < x_max) {
    const auto x_float = x_fixed.to_real();

    const auto y_curve = test_gain(x_float).f;
    const auto y_table = spline::eval(&spline, x_fixed.raw);
    x_fixed += dx;

    const auto expected = y_curve;
    const auto actual = Fixed::from_raw(y_table).to_real();

    accuracy_metrics.sample(x_float, actual, expected);
  }
  std::cout << "x1: " << x_fixed << " (" << x_fixed.raw << " fixed)"
            << std::endl;

  std::cout << accuracy_metrics << std::endl;

  ASSERT_LE(accuracy_metrics.mse_abs(), 7.92163e-06L);
  ASSERT_LE(accuracy_metrics.rmse_abs(), 1.87908e-06L);
  ASSERT_LE(accuracy_metrics.mse_rel(), 2.08249e-05L);
  ASSERT_LE(accuracy_metrics.rmse_rel(), 5.498625e-08L);
}

TEST(spline, sensitivity_vs_gain) {
  const auto curve = SynchronousCurve{8.0L, 0.5L, 10.5L, 0.5L};
  const auto sensitivity_spline =
      spline::create_spline(FromSensitivity{curve}, 1.0L);
  const auto gain_spline = spline::create_spline(FromGain{curve}, 1.0L);

  ASSERT_EQ(sensitivity_spline.v_to_x, gain_spline.v_to_x);
  ASSERT_EQ(sensitivity_spline.x_geometric_limit,
            gain_spline.x_geometric_limit);
  ASSERT_EQ(sensitivity_spline.x_runout_limit, gain_spline.x_runout_limit);
  ASSERT_EQ(sensitivity_spline.runout_width_log2,
            gain_spline.runout_width_log2);

  const auto x_max = Fixed::from_raw(sensitivity_spline.x_geometric_limit);

  const auto dv = Fixed{1.0e-3L};
  std::cout << "dv: " << dv << " (" << dv.raw << " fixed)" << std::endl;

  auto v = Fixed(0);
  auto x = Fixed::from_raw(spline::map_v_to_x(&sensitivity_spline, v.raw));
  std::cout << "x0: " << x << " (" << x.raw << " fixed)" << std::endl;

  const auto knot_locator = spline::KnotLocator{};

  auto accuracy_metrics = AccuracyMetrics{};
  while (v < x_max) {
    const auto xSx_viaTs =
        Fixed::from_raw(spline::eval(&sensitivity_spline, x.raw));

    const auto spline_coords = spline::resolve_x(x.raw);
    const auto gain_segment = gain_spline.segments[spline_coords.segment_index];

    // Manual horner's.
    const auto* const c = gain_segment.coeffs;
    auto dTg = 3 * Fixed::from_raw(c[0]);
    const auto t = Fixed::from_raw(spline_coords.t);
    dTg = dTg.fma(t, 2 * Fixed::from_raw(c[1]));
    dTg = dTg.fma(t, Fixed::from_raw(c[2]));

    const auto segment_width =
        Fixed::from_raw(knot_locator(spline_coords.segment_index + 1) -
                        knot_locator(spline_coords.segment_index));

    const auto xSx_viaTg = x * dTg / segment_width;

    accuracy_metrics.sample(x.to_real(), xSx_viaTs.to_real(),
                            xSx_viaTg.to_real());

    v += dv;
    x = Fixed::from_raw(spline::map_v_to_x(&sensitivity_spline, v.raw));
  }
  std::cout << "x1: " << x << " (" << x.raw << " fixed)" << std::endl;

  std::cout << accuracy_metrics << std::endl;

  ASSERT_LE(accuracy_metrics.mse_abs(), 0.000411519L);
  ASSERT_LE(accuracy_metrics.rmse_abs(), 0.000130904L);
  ASSERT_LE(accuracy_metrics.mse_rel(), 0.000624302L);
  ASSERT_LE(accuracy_metrics.rmse_rel(), 1.788635e-06L);
}

}  // namespace
}  // namespace curves
