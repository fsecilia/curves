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

  auto max_abs_err = 0.0L;
  auto max_rel_err = 0.0L;
  auto max_abs_err_x = 0.0L;
  auto max_rel_err_x = 0.0L;
  auto sse_abs = 0.0L;
  auto sse_rel = 0.0L;
  auto num_samples = 0;
  while (x_fixed < x_max) {
    const auto x_float = x_fixed.to_real();

    const auto y_curve = sensitivity(x_float).f;
    const auto y_table = spline::eval(&spline, x_fixed.raw);
    x_fixed += dx;

    const auto expected = y_curve;
    const auto actual = Fixed::from_raw(y_table).to_real();

    ++num_samples;
    if (!expected) continue;  // skip near 0 so rel doesn't explode

    const auto abs_err = std::abs(actual - expected);
    if (max_abs_err < abs_err) {
      max_abs_err = abs_err;
      max_abs_err_x = x_float;
    }
    sse_abs += abs_err * abs_err;

    const auto rel_err = std::abs(abs_err / expected);
    if (max_rel_err < rel_err) {
      max_rel_err = rel_err;
      max_rel_err_x = x_float;
    }
    sse_rel += rel_err * rel_err;
  }
  std::cout << "x1: " << x_fixed << " (" << x_fixed.raw << " fixed)"
            << std::endl;

  const auto mse_abs = sse_abs / num_samples;
  const auto mse_rel = sse_rel / num_samples;
  std::cout << "Max Abs Error: " << max_abs_err << " (x = " << max_abs_err_x
            << ")"
            << "\nSSE Abs: " << sse_abs << "\nMSE Abs: " << mse_abs
            << "\nRMSE Abs: " << std::sqrt(mse_abs)
            << "\nMax Rel Error: " << max_rel_err << " (x = " << max_rel_err_x
            << ")"
            << "\nSSE Rel: " << sse_rel << "\nMSE Rel: " << mse_rel
            << "\nRMSE Rel: " << std::sqrt(mse_rel) << std::endl;
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

  auto max_abs_err = 0.0L;
  auto max_rel_err = 0.0L;
  auto max_abs_err_x = 0.0L;
  auto max_rel_err_x = 0.0L;
  auto sse_abs = 0.0L;
  auto sse_rel = 0.0L;
  auto num_samples = 0;
  while (x_fixed < x_max) {
    const auto x_float = x_fixed.to_real();

    const auto y_curve = test_gain(x_float).f;
    const auto y_table = spline::eval(&spline, x_fixed.raw);
    x_fixed += dx;

    const auto expected = y_curve;
    const auto actual = Fixed::from_raw(y_table).to_real();

    ++num_samples;
    if (!expected) continue;  // skip near 0 so rel doesn't explode

    const auto abs_err = std::abs(actual - expected);
    if (max_abs_err < abs_err) {
      max_abs_err = abs_err;
      max_abs_err_x = x_float;
    }
    sse_abs += abs_err * abs_err;

    const auto rel_err = std::abs(abs_err / expected);
    if (max_rel_err < rel_err) {
      max_rel_err = rel_err;
      max_rel_err_x = x_float;
    }
    sse_rel += rel_err * rel_err;
  }
  std::cout << "x1: " << x_fixed << " (" << x_fixed.raw << " fixed)"
            << std::endl;

  const auto mse_abs = sse_abs / num_samples;
  const auto mse_rel = sse_rel / num_samples;
  std::cout << "Max Abs Error: " << max_abs_err << " (x = " << max_abs_err_x
            << ")"
            << "\nSSE Abs: " << sse_abs << "\nMSE Abs: " << mse_abs
            << "\nRMSE Abs: " << std::sqrt(mse_abs)
            << "\nMax Rel Error: " << max_rel_err << " (x = " << max_rel_err_x
            << ")"
            << "\nSSE Rel: " << sse_rel << "\nMSE Rel: " << mse_rel
            << "\nRMSE Rel: " << std::sqrt(mse_rel) << std::endl;
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
  auto x =
      Fixed::from_raw(spline::transform_v_to_x(&sensitivity_spline, v.raw));
  std::cout << "x0: " << x << " (" << x.raw << " fixed)" << std::endl;

  const auto knot_locator = spline::KnotLocator{};

  auto accuracy_metrics = AccuracyMetrics{};
  while (v < x_max) {
    const auto xSx_viaTs =
        Fixed::from_raw(spline::eval(&sensitivity_spline, x.raw));

    s64 segment_index;
    s64 t;
    spline::locate_segment(x.raw, &segment_index, &t);
    const auto gain_segment = gain_spline.segments[segment_index];
    const auto* const c = gain_segment.coeffs;
    auto dTg = 3 * c[0];
    dTg = (s64)(((s128)dTg * t + SPLINE_FRAC_HALF) >> SPLINE_FRAC_BITS) +
          2 * c[1];
    dTg = (s64)(((s128)dTg * t + SPLINE_FRAC_HALF) >> SPLINE_FRAC_BITS) + c[2];

    const auto segment_width = Fixed::from_raw(knot_locator(segment_index + 1) -
                                              knot_locator(segment_index));

    const auto xSx_viaTg = x * Fixed::from_raw(dTg) / segment_width;

    accuracy_metrics.sample(x.to_real(), xSx_viaTs.to_real(),
                            xSx_viaTg.to_real());

    v += dv;
    x = Fixed::from_raw(spline::transform_v_to_x(&sensitivity_spline, v.raw));
  }
  std::cout << "x1: " << x << " (" << x.raw << " fixed)" << std::endl;

  std::cout << accuracy_metrics << std::endl;
}

}  // namespace
}  // namespace curves
