// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include <curves/testing/test.hpp>
#include <curves/io.hpp>
#include <curves/spline.hpp>

namespace curves {
namespace {

TEST(spline_set, synchronous_as_transfer) {
  const auto crossover = 8.3L;
  auto sensitivity = SynchronousCurve{10.0L, 1.0L, crossover, 0.5L};

  const auto x_max = 128.0L;
  auto spline_set = generate_transfer_splines(sensitivity, crossover, x_max);

  const auto dx_fixed = to_fixed(1.0e-3L, CURVES_SPLINE_FRAC_BITS);
  // const auto x_max_fixed = to_fixed(x_max, CURVES_SPLINE_FRAC_BITS);
  const auto x_cusp_fixed = to_fixed(crossover, CURVES_SPLINE_FRAC_BITS);
  auto x_fixed = x_cusp_fixed - dx_fixed * 200;
  auto max_abs_err = 0.0L;
  auto max_rel_err = 0.0L;
  auto max_abs_err_x = 0.0L;
  auto max_rel_err_x = 0.0L;
  auto sse_abs = 0.0L;
  auto sse_rel = 0.0L;
  auto num_samples = 0;
  while (x_fixed < x_cusp_fixed + dx_fixed * 200) {
    const auto x_float =
        to_float<long double>(x_fixed, CURVES_SPLINE_FRAC_BITS);

    const auto y_curve = x_float * sensitivity(x_float).f;
    const auto y_table = eval_transfer_curve(&spline_set, x_fixed);
    x_fixed += dx_fixed;

    const auto expected = y_curve;
    const auto actual = to_float<long double>(y_table, CURVES_SPLINE_FRAC_BITS);

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

  const auto mse_abs = sse_abs / num_samples;
  const auto mse_rel = sse_rel / num_samples;
  std::cout << "Max Abs Error: " << max_abs_err << " (" << max_abs_err_x << ")"
            << "\nSSE Abs: " << sse_abs << "\nMSE Abs: " << mse_abs
            << "\nRMSE Abs: " << std::sqrt(mse_abs)
            << "\nMax Rel Error: " << max_rel_err << " (" << max_rel_err_x
            << ")"
            << "\nSSE Rel: " << sse_rel << "\nMSE Rel: " << mse_rel
            << "\nRMSE Rel: " << std::sqrt(mse_rel) << std::endl;
}

struct SplineTest : Test {};

TEST_F(SplineTest, uniform_index_end) {}

#if 0
struct SegmentLayoutParam {
  real_t crossover;
  SegmentLayout expected;

  friend auto operator<<(std::ostream& out, const SegmentLayoutParam& src)
      -> std::ostream& {
    return out << "{" << src.crossover << ", {" << src.expected.segment_width
               << ", " << src.expected.x_max << "}}";
  }
};

struct SegmentLayoutTest : TestWithParam<SegmentLayoutParam> {
  using Sut = SegmentLayout;
  const Sut sut = create_segment_layout(GetParam().crossover);
};

TEST_P(SegmentLayoutTest, segment_width) {
  const auto expected = GetParam().expected.segment_width;

  const auto actual = sut.segment_width;

  ASSERT_EQ(expected, actual);
}

TEST_P(SegmentLayoutTest, x_max) {
  const auto expected = GetParam().expected.x_max;

  const auto actual = sut.x_max;

  ASSERT_EQ(expected, actual);
}

const auto N = real_t{CURVES_SPLINE_NUM_SEGMENTS};
const auto TARGET = real_t{x_max_ideal};
const auto ideal_segment_width =
    x_max_ideal / real_t{CURVES_SPLINE_NUM_SEGMENTS};

const SegmentLayoutParam segment_layout_params[] = {
    {1.5, {ideal_segment_width / 1.5, 1.5 * x_max_ideal}},

    {1, {ideal_segment_width, x_max_ideal}},
    {5, {ideal_segment_width, x_max_ideal}},

    {1.5, {ideal_segment_width / 1.5, 1.5 * x_max_ideal}},
    {0.75, {ideal_segment_width / 0.75, 0.75 * x_max_ideal}},

    {TARGET / 4, {N / TARGET, TARGET}},
    {0.1, {1.0 / 0.1, 0.1 * N}},
    {TARGET * 2, {(N - 1) / (TARGET * 2), (TARGET * 2 / (N - 1)) * N}},

    {(TARGET / N) * 10.1,
     {10.0 / ((TARGET / N) * 10.1), (((TARGET / N) * 10.1) / 10.0) * N}}

};

INSTANTIATE_TEST_SUITE_P(all_cases, SegmentLayoutTest,
                         ValuesIn(segment_layout_params));
#endif

}  // namespace
}  // namespace curves
