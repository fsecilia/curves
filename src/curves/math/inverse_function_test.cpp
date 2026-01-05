// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include "inverse_function.hpp"
#include <curves/testing/test.hpp>
#include <cmath>

namespace curves {
namespace {

struct TestVector {
  std::string_view description;
  real_t x;
  real_t tolerance;

  friend auto operator<<(std::ostream& out, const TestVector& src)
      -> std::ostream& {
    return out << "{.x = " << src.x << ", .tolerance = " << src.tolerance
               << "}";
  }
};
using InverseFunctionTestVectorNameGenerator = TestNameGenerator<TestVector>;

struct InverseFunctionTest : TestWithParam<TestVector> {
  const real_t x = GetParam().x;
  const real_t tolerance = GetParam().tolerance;
};

// ----------------------------------------------------------------------------
// Exp, increasing
// ----------------------------------------------------------------------------

struct InverseFunctionTestExp : InverseFunctionTest {};

TEST_P(InverseFunctionTestExp, exp) {
  ASSERT_NEAR(
      x,
      inverse_via_partition([](auto&& x) { return std::exp(x); }, std::exp(x)),
      tolerance);
}

const TestVector exp_vectors[] = {
    TestVector{"exp(0)", 0.0L, 1e-10L},
    TestVector{"exp(e)", static_cast<real_t>(M_E), 1e-10L},
    TestVector{"exp(pi)", static_cast<real_t>(M_PI), 1e-10L},
    TestVector{"exp(5)", 5.0L, 1e-10L},
};

INSTANTIATE_TEST_SUITE_P(exp_vectors, InverseFunctionTestExp,
                         ValuesIn(exp_vectors),
                         InverseFunctionTestVectorNameGenerator{});

// ----------------------------------------------------------------------------
// Log, increasing
// ----------------------------------------------------------------------------

struct InverseFunctionTestLog : InverseFunctionTest {};

TEST_P(InverseFunctionTestLog, log) {
  ASSERT_NEAR(
      x,
      inverse_via_partition([](auto&& x) { return std::log(x); }, std::log(x)),
      tolerance);
}

const TestVector log_vectors[] = {
    TestVector{"log(1)", 1.0L, 1e-10L},
    TestVector{"log(e)", static_cast<real_t>(M_E), 1e-10L},
    TestVector{"log(pi)", static_cast<real_t>(M_PI), 1e-10L},
    TestVector{"log(5)", 5.0L, 1e-10L},
};

INSTANTIATE_TEST_SUITE_P(log_vectors, InverseFunctionTestLog,
                         ValuesIn(log_vectors),
                         InverseFunctionTestVectorNameGenerator{});

// ----------------------------------------------------------------------------
// 1/x, decreasing
// ----------------------------------------------------------------------------

struct InverseFunctionTestInverseX : InverseFunctionTest {};

TEST_P(InverseFunctionTestInverseX, inverse_x) {
  ASSERT_NEAR(
      x, inverse_via_partition([](auto&& x) { return 1.0L / x; }, 1.0L / x),
      tolerance);
}

const TestVector inverse_x_vectors[] = {
    TestVector{"1/1", 1.0L, 1e-10L},
    TestVector{"1/e", static_cast<real_t>(M_E), 1e-10L},
    TestVector{"1/pi", static_cast<real_t>(M_PI), 1e-10L},
    TestVector{"1/5", 5.0L, 1e-10L},
};

INSTANTIATE_TEST_SUITE_P(inverse_x_vectors, InverseFunctionTestInverseX,
                         ValuesIn(inverse_x_vectors),
                         InverseFunctionTestVectorNameGenerator{});

// ----------------------------------------------------------------------------
// Pow, steep
// ----------------------------------------------------------------------------

struct InverseFunctionTestPowSteep : InverseFunctionTest {};

TEST_P(InverseFunctionTestPowSteep, pow) {
  ASSERT_NEAR(x,
              inverse_via_partition([](auto&& x) { return std::pow(x, 10); },
                                    std::pow(x, 10)),
              tolerance);
}

// Steep Gradient
// Small changes in x = large changes in y.
// Tests if bracket expansion explodes too fast.
const TestVector pow_steep_vectors[] = {
    TestVector{"pow(0.5, 10)", 0.5L, 1e-9L},  // y is small
    TestVector{"pow(2.0, 10)", 2.0L, 1e-9L},  // y is large
};

INSTANTIATE_TEST_SUITE_P(pow_steep_vectors, InverseFunctionTestPowSteep,
                         ValuesIn(pow_steep_vectors),
                         InverseFunctionTestVectorNameGenerator{});

// ----------------------------------------------------------------------------
// Pow, shallow
// ----------------------------------------------------------------------------

struct InverseFunctionTestPowShallow : InverseFunctionTest {};

TEST_P(InverseFunctionTestPowShallow, pow) {
  ASSERT_NEAR(x,
              inverse_via_partition([](auto&& x) { return std::pow(x, 0.1); },
                                    std::pow(x, 0.1)),
              tolerance);
}

// Shallow Gradient
// Large changes in x = small changes in y.
// Tests if binary search terminates when x is wiggling but y is barely moving.
const TestVector pow_shallow_vectors[] = {
    TestVector{"pow(100, 0.1)", 100.0L, 1e-9L},  // y is large
    TestVector{"pow(0.01, 0.1)", 0.01L, 1e-9L},  // y is small
};

INSTANTIATE_TEST_SUITE_P(pow_shallow_vectors, InverseFunctionTestPowShallow,
                         ValuesIn(pow_shallow_vectors),
                         InverseFunctionTestVectorNameGenerator{});

// ----------------------------------------------------------------------------
// Linear
// ----------------------------------------------------------------------------

struct InverseFunctionTestLinear : InverseFunctionTest {};

TEST_P(InverseFunctionTestLinear, linear) {
  ASSERT_NEAR(x, inverse_via_partition([](auto&& x) { return x; }, x),
              tolerance);
}

// Small Integers
// Tests search resolves x for small, distinct integers.
const TestVector linear_vectors[] = {
    TestVector{"100.0", 100.0L, 1e-9L},  // y is large
    TestVector{"0.01", 0.01L, 1e-9L},    // y is small
};

INSTANTIATE_TEST_SUITE_P(linear_vectors, InverseFunctionTestLinear,
                         ValuesIn(linear_vectors),
                         InverseFunctionTestVectorNameGenerator{});

// ----------------------------------------------------------------------------
// Offset
// ----------------------------------------------------------------------------

struct InverseFunctionTestOffset : InverseFunctionTest {};

TEST_P(InverseFunctionTestOffset, offset) {
  ASSERT_NEAR(x, inverse_via_partition([](auto&& x) { return x + 10; }, x + 10),
              tolerance);
}

// Small Integers
// Tests search resolves x for small, distinct integers.
const TestVector offset_vectors[] = {
    TestVector{"0 + 10", 0.0L, 1e-9L},  // y is large
    TestVector{"5 + 10", 5.0L, 1e-9L},  // y is small
};

INSTANTIATE_TEST_SUITE_P(offset_vectors, InverseFunctionTestOffset,
                         ValuesIn(offset_vectors),
                         InverseFunctionTestVectorNameGenerator{});

}  // namespace
}  // namespace curves
