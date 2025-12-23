// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include "integration.hpp"
#include <curves/testing/test.hpp>
#include <cmath>
#include <functional>
#include <string_view>

namespace curves {
namespace {

// ----------------------------------------------------------------------------
// Test Polynomials
// ----------------------------------------------------------------------------

// These are defined as free functions so we can store pointers to them in
// test vectors. Each returns a Jet for Hermite methods; for Gauss, we
// extract just .f.

inline auto poly_constant(real_t /*x*/) -> Jet { return {2.0L, 0.0L}; }
inline auto poly_linear(real_t x) -> Jet { return {x, 1.0L}; }
inline auto poly_quadratic(real_t x) -> Jet { return {x * x, 2.0L * x}; }
inline auto poly_cubic(real_t x) -> Jet { return {x * x * x, 3.0L * x * x}; }
inline auto poly_quartic(real_t x) -> Jet {
  return {x * x * x * x, 4.0L * x * x * x};
}
inline auto poly_quintic(real_t x) -> Jet {
  return {x * x * x * x * x, 5.0L * x * x * x * x};
}

// Arbitrary cubic: f(x) = x^3 - 2x^2 + 4x + 1
inline auto poly_arbitrary_cubic(real_t x) -> Jet {
  return {x * x * x - 2.0L * x * x + 4.0L * x + 1.0L,
          3.0L * x * x - 4.0L * x + 4.0L};
}

// Transcendental functions for convergence testing.
inline auto func_sin(real_t x) -> Jet { return {std::sin(x), std::cos(x)}; }
inline auto func_exp(real_t x) -> Jet { return {std::exp(x), std::exp(x)}; }

// ----------------------------------------------------------------------------
// Test Vectors
// ----------------------------------------------------------------------------

// For function-based tests, we store a pointer to the Jet-returning function.
// Tests call it directly for Hermite methods, or wrap it for Gauss.
using JetFn = Jet (*)(real_t);

struct IntegralTestVector {
  std::string_view description;
  JetFn f;
  real_t a;
  real_t b;
  real_t expected;
  real_t tol_trapezoid4;  // Tolerance for O(h^4) method
  real_t tol_trapezoid8;  // Tolerance for O(h^8) method
  real_t tol_gauss3;      // Tolerance for O(h^6) method
  real_t tol_gauss5;      // Tolerance for O(h^10) method
};

// Creates test vectors with uniform tight tolerance.
constexpr auto exact_test(std::string_view desc, JetFn f, real_t a, real_t b,
                          real_t expected) -> IntegralTestVector {
  return {desc, f, a, b, expected, 1e-15L, 1e-15L, 1e-15L, 1e-15L};
}

const IntegralTestVector kPolynomialTests[] = {
    // Corrected trapezoidal is exact up to cubic.
    // Gauss-3 is exact up to degree 5, Gauss-5 up to degree 9.

    exact_test("Constant", poly_constant, 0.0L, 1.0L, 2.0L),
    exact_test("Linear", poly_linear, 0.0L, 1.0L, 0.5L),
    exact_test("Quadratic [0,1]", poly_quadratic, 0.0L, 1.0L, 1.0L / 3.0L),
    exact_test("Quadratic [0,2]", poly_quadratic, 0.0L, 2.0L, 8.0L / 3.0L),
    exact_test("Cubic [0,1]", poly_cubic, 0.0L, 1.0L, 0.25L),
    exact_test("Arbitrary Cubic [0,3]", poly_arbitrary_cubic, 0.0L, 3.0L,
               23.25L),
    exact_test(
        "Negative Linear", [](real_t x) -> Jet { return {-x, -1.0L}; }, 0.0L,
        1.0L, -0.5L),

    // Quartic: trapezoid4 is approximate, trapezoid8/gauss are exact.
    IntegralTestVector{
        .description = "Quartic [0,1]",
        .f = poly_quartic,
        .a = 0.0L,
        .b = 1.0L,
        .expected = 1.0L / 5.0L,
        .tol_trapezoid4 = 4e-2L,
        .tol_trapezoid8 = 1e-15L,
        .tol_gauss3 = 1e-15L,
        .tol_gauss5 = 1e-15L,
    },

    // Quintic: only gauss3+ are exact.
    IntegralTestVector{
        .description = "Quintic [0,1]",
        .f = poly_quintic,
        .a = 0.0L,
        .b = 1.0L,
        .expected = 1.0L / 6.0L,
        .tol_trapezoid4 = 1e-1L,
        .tol_trapezoid8 = 1e-4L,
        .tol_gauss3 = 1e-15L,
        .tol_gauss5 = 1e-15L,
    },
};

const IntegralTestVector kTranscendentalTests[] = {
    // sin(x) from 0 to pi: exact = 2
    IntegralTestVector{
        .description = "sin [0,pi]",
        .f = func_sin,
        .a = 0.0L,
        .b = 3.14159265358979323846L,
        .expected = 2.0L,
        .tol_trapezoid4 = 4e-1L,
        .tol_trapezoid8 = 1.7e-5L,
        .tol_gauss3 = 1.8e-3L,
        .tol_gauss5 = 1.4e-7L,
    },

    // exp(x) from 0 to 1: exact = e - 1
    IntegralTestVector{
        .description = "exp [0,1]",
        .f = func_exp,
        .a = 0.0L,
        .b = 1.0L,
        .expected = 1.71828182845904523536L,
        .tol_trapezoid4 = 1e-2L,
        .tol_trapezoid8 = 1.05e-9L,
        .tol_gauss3 = 1e-5L,
        .tol_gauss5 = 1e-10L,
    },
};

// ----------------------------------------------------------------------------
// Parametrized Tests
// ----------------------------------------------------------------------------

class IntegralTest : public ::testing::TestWithParam<IntegralTestVector> {};

TEST_P(IntegralTest, trapezoid4_samples) {
  const auto& p = GetParam();
  const auto samples = Trapezoid4Samples{p.f(p.a), p.f(p.b)};
  const auto result = trapezoid4(p.a, p.b, samples);
  EXPECT_NEAR(result, p.expected, p.tol_trapezoid4) << p.description;
}

TEST_P(IntegralTest, trapezoid4_function) {
  const auto& p = GetParam();
  const auto result = trapezoid4(p.f, p.a, p.b);
  EXPECT_NEAR(result, p.expected, p.tol_trapezoid4) << p.description;
}

TEST_P(IntegralTest, trapezoid8_function) {
  const auto& p = GetParam();
  const auto result = trapezoid8(p.f, p.a, p.b);
  EXPECT_NEAR(result, p.expected, p.tol_trapezoid8) << p.description;
}

TEST_P(IntegralTest, gauss3_samples) {
  const auto& p = GetParam();

  // Wrap the Jet function to extract just the value.
  auto value_fn = [&](real_t x) { return p.f(x).f; };

  const auto nodes = gauss3_nodes(p.a, p.b);
  const auto samples =
      Gauss3Samples{value_fn(nodes[0]), value_fn(nodes[1]), value_fn(nodes[2])};
  const auto result = gauss3(p.a, p.b, samples);
  EXPECT_NEAR(result, p.expected, p.tol_gauss3) << p.description;
}

TEST_P(IntegralTest, gauss3_function) {
  const auto& p = GetParam();
  auto value_fn = [&](real_t x) { return p.f(x).f; };
  const auto result = gauss3(value_fn, p.a, p.b);
  EXPECT_NEAR(result, p.expected, p.tol_gauss3) << p.description;
}

TEST_P(IntegralTest, gauss4_function) {
  const auto& p = GetParam();
  auto value_fn = [&](real_t x) { return p.f(x).f; };
  const auto result = gauss4(value_fn, p.a, p.b);
  // gauss4 is O(h^8), between gauss3 and gauss5 in accuracy.
  const auto tol = std::sqrt(p.tol_gauss3 * p.tol_gauss5);
  EXPECT_NEAR(result, p.expected, tol) << p.description;
}

TEST_P(IntegralTest, gauss5_function) {
  const auto& p = GetParam();
  auto value_fn = [&](real_t x) { return p.f(x).f; };
  const auto result = gauss5(value_fn, p.a, p.b);
  EXPECT_NEAR(result, p.expected, p.tol_gauss5) << p.description;
}

// Name generator for readable test output.
template <typename TestVector>
struct TestNameGenerator {
  auto operator()(const ::testing::TestParamInfo<TestVector>& info)
      -> std::string {
    auto s = std::string{info.param.description};
    std::replace_if(
        s.begin(), s.end(), [](char c) { return !std::isalnum(c); }, '_');
    return s;
  }
};

using IntegralTestVectorNameGenerator = TestNameGenerator<IntegralTestVector>;

INSTANTIATE_TEST_SUITE_P(Polynomials, IntegralTest,
                         ::testing::ValuesIn(kPolynomialTests),
                         IntegralTestVectorNameGenerator{});

INSTANTIATE_TEST_SUITE_P(Transcendental, IntegralTest,
                         ::testing::ValuesIn(kTranscendentalTests),
                         IntegralTestVectorNameGenerator{});

// ----------------------------------------------------------------------------
// Direct Sample-Based Tests (for the existing test vector format)
// ----------------------------------------------------------------------------

struct LegacyTestVector {
  std::string_view description;
  real_t h;
  Jet start;
  Jet end;
  real_t expected;
};

class LegacyIntegrationTest
    : public ::testing::TestWithParam<LegacyTestVector> {};

TEST_P(LegacyIntegrationTest, trapezoid4_legacy) {
  const auto& p = GetParam();

  // Reconstruct a and b from h (assumes a = 0).
  const auto a = 0.0L;
  const auto b = p.h;

  const auto result = trapezoid4(a, b, Trapezoid4Samples{p.start, p.end});
  EXPECT_NEAR(result, p.expected, 1e-15L) << p.description;
}

const LegacyTestVector kLegacyPolynomialTests[] = {
    {"Constant Function", 1.0L, {2.0L, 0.0L}, {2.0L, 0.0L}, 2.0L},
    {"Linear Function", 1.0L, {0.0L, 1.0L}, {1.0L, 1.0L}, 0.5L},
    {"Quadratic", 1.0L, {0.0L, 0.0L}, {1.0L, 2.0L}, 1.0L / 3.0L},
    {"Cubic", 1.0L, {0.0L, 0.0L}, {1.0L, 3.0L}, 0.25L},
    {"Quadratic [0,2]", 2.0L, {0.0L, 0.0L}, {4.0L, 4.0L}, 8.0L / 3.0L},
    {"Negative Slope", 1.0L, {0.0L, -1.0L}, {-1.0L, -1.0L}, -0.5L},
    {"Arbitrary Cubic", 3.0L, {1.0L, 4.0L}, {22.0L, 19.0L}, 23.25L},
};

using LegacyTestVectorNameGenerator = TestNameGenerator<LegacyTestVector>;

INSTANTIATE_TEST_SUITE_P(LegacyPolynomials, LegacyIntegrationTest,
                         ::testing::ValuesIn(kLegacyPolynomialTests),
                         LegacyTestVectorNameGenerator{});

}  // namespace
}  // namespace curves
