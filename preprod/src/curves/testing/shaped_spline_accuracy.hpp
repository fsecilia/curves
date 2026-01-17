// shaped_spline_accuracy_test.cpp

#include <curves/math/curves/spline/shaped_spline_builder.hpp>
#include <curves/math/input_shaping_view.hpp>
#include <curves/math/integration.hpp>
#include <curves/math/shaped_spline_view.hpp>
#include <curves/testing/error_metrics.hpp>
#include <iostream>

namespace curves {

// ============================================================================
// Accuracy Measurement
// ============================================================================

struct AccuracyResult {
  real_t max_T_error;
  real_t max_dT_error;
  real_t rms_T_error;
  real_t rms_dT_error;
  real_t max_T_error_at_v;
  real_t max_dT_error_at_v;
};

/*!
  Measure accuracy of a shaped spline against an oracle.

  Samples sequentially from 0 to v_max, which allows stateful oracles
  (like GainOracle) to work efficiently.
*/
template <typename Oracle>
auto measure_accuracy(const shaped_spline& spline, Oracle& oracle,
                      real_t offset, real_t v_max, int num_samples = 10000)
    -> AccuracyResult {
  ShapedSplineView view{&spline};

  AccuracyResult result{};
  real_t sum_T_sq = 0;
  real_t sum_dT_sq = 0;

  for (int i = 0; i <= num_samples; ++i) {
    const real_t v = v_max * i / num_samples;

    auto [T_expected, dT_expected] = oracle(v);
    T_expected += offset * v;
    dT_expected += offset;

    const auto [T_actual, dT_actual, d2T_actual] = view(v);

    const real_t T_err = std::abs(T_actual - T_expected);
    const real_t dT_err = std::abs(dT_actual - dT_expected);

    sum_T_sq += T_err * T_err;
    sum_dT_sq += dT_err * dT_err;

    if (T_err > result.max_T_error) {
      result.max_T_error = T_err;
      result.max_T_error_at_v = v;
    }
    if (dT_err > result.max_dT_error) {
      result.max_dT_error = dT_err;
      result.max_dT_error_at_v = v;
    }
  }

  result.rms_T_error = std::sqrt(sum_T_sq / (num_samples + 1));
  result.rms_dT_error = std::sqrt(sum_dT_sq / (num_samples + 1));

  return result;
}

// ============================================================================
// Example Generating Curves for Testing
// ============================================================================

/// Power law: f(x) = scale × x^gamma
struct PowerLaw {
  real_t gamma = 1.0L;
  real_t scale = 1.0L;

  template <typename Value>
  auto operator()(const Value& x) const noexcept -> Value {
    using math::pow;
    if (x <= 0) return 0;
    return scale * pow(x, gamma);
  }

  auto value(real_t x) const -> real_t {
    if (x <= 0) return 0;
    return scale * std::pow(x, gamma);
  }

  auto derivative(real_t x) const -> real_t {
    if (x <= 0) return (gamma == 1.0L) ? scale : 0;
    return scale * gamma * std::pow(x, gamma - 1);
  }

  auto gain(real_t x) const -> real_t {
    // G(x) = T'(x) = (gamma + 1) * scale * x^gamma
    if (x <= 0) return 0;
    return (gamma + 1) * scale * std::pow(x, gamma);
  }
};

/// Log curve: f(x) = scale × log(1 + rate × x)
struct Log1p {
  real_t scale = 1.0L;
  real_t rate = 1.0L;

  template <typename Value>
  auto operator()(const Value& x) const noexcept -> Value {
    using math::log1p;
    if (x <= 0) return 0;
    return scale * log1p(rate * x);
  }

  auto value(real_t x) const -> real_t { return scale * std::log1p(rate * x); }

  auto derivative(real_t x) const -> real_t {
    return scale * rate / (1 + rate * x);
  }

  auto gain(real_t x) const -> real_t {
    // S(x) = scale * log(1 + rate*x)
    // T(x) = x * S(x) = x * scale * log(1 + rate*x)
    // G(x) = T'(x) = scale * log(1 + rate*x) + x * scale * rate / (1 + rate*x)
    //              = scale * (log(1 + rate*x) + x*rate / (1 + rate*x))
    const real_t rx = rate * x;
    return scale * (std::log1p(rx) + rx / (1 + rx));
  }
};

}  // namespace curves
