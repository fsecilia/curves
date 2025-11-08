// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include <curves/kernel/common_test.hpp>
#include <ostream>

extern "C" {
#include <curves/kernel/driver/math64.h>
}  // extern "C"

namespace curves {
namespace {

const auto min = INT64_MIN;
const auto max = INT64_MAX;

// ----------------------------------------------------------------------------
// Parameterized Test
// ----------------------------------------------------------------------------

struct param_t {
  struct input_t {
    int64_t left;
    int64_t right;
    unsigned shift;

    friend auto operator<<(std::ostream& out, const input_t& src)
        -> std::ostream& {
      return out << "{" << src.left << ", " << src.right << ", " << src.shift
                 << "}";
    }
  };

  friend auto operator<<(std::ostream& out, const param_t& src)
      -> std::ostream& {
    return out << "{" << src.input << ", " << src.expected << "}";
  }

  input_t input;
  int64_t expected;
};

struct int128_test_t : TestWithParam<param_t> {};

// ----------------------------------------------------------------------------
// Multiplication
// ----------------------------------------------------------------------------

struct int128_test_mul_i64_i64_shr_t : int128_test_t {};

TEST_P(int128_test_mul_i64_i64_shr_t, result) {
  const auto& input = GetParam().input;
  ASSERT_EQ(curves_mul_i64_i64_shr(input.left, input.right, input.shift),
            GetParam().expected);
}

const param_t mul_params[] = {
    // simple zeros
    {{0, 1, 0}, 0},
    {{0, -1, 0}, 0},
    {{-1, 0, 0}, 0},

    // simple positive
    {{1, 1, 1}, 0},
    {{1, 1, 0}, 1},
    {{1LL << 62, 1, 0}, 1LL << 62},

    // small positive
    {{15, 26, 2}, 15 * 26 >> 2},
    {{89, 11, 3}, 89 * 11 >> 3},

    // fixed point values
    {{1447LL << 32, 13LL << 32, 32}, 1447LL * 13LL << 32},

    // large positive values with shifts
    {{1LL << 62, 1, 1}, 1LL << 61},
    {{1LL << 62, 1, 61}, 2},
    {{1LL << 62, 1, 62}, 1},
    {{1LL << 62, 1, 63}, 0},
    {{1LL << 61, 2, 62}, 1},
    {{1LL << 60, 4, 62}, 1},

    // values requiring more than 64 bits internally
    {{1LL << 32, 1LL << 32, 32}, 1LL << 32},
    {{1LL << 40, 1LL << 40, 48}, 1LL << 32},
    {{1LL << 50, 1LL << 50, 68}, 1LL << 32},
    {{1000000000LL, 1000000000LL, 20}, 953674316406LL},
    {{100LL << 32, 200LL << 32, 63}, 100LL * 200 << 1},

    // simple negatives
    {{-1, 1, 0}, -1},
    {{1, -1, 0}, -1},
    {{-1, -1, 0}, 1},
    {{-1, 100, 0}, -100},
    {{100, -1, 0}, -100},

    // negative * positive
    {{-15, 26, 2}, -15 * 26 >> 2},
    {{-89, 11, 3}, -89 * 11 >> 3},

    // positive * negative
    {{15, -26, 2}, 15 * -26 >> 2},
    {{89, -11, 3}, 89 * -11 >> 3},

    // negative * negative
    {{-15, -26, 2}, 15 * 26 >> 2},
    {{-89, -11, 3}, 89 * 11 >> 3},

    // negative fixed point
    {{-1447LL << 32, 13LL << 32, 32}, -1447LL * 13LL << 32},
    {{1447LL << 32, -13LL << 32, 32}, -1447LL * 13LL << 32},
    {{-1447LL << 32, -13LL << 32, 32}, 1447LL * 13LL << 32},

    // large negative values
    {{-(1LL << 62), 1, 0}, -(1LL << 62)},
    {{1, -(1LL << 62), 0}, -(1LL << 62)},
    {{-(1LL << 62), -1, 0}, 1LL << 62},
    {{-(1LL << 61), 2, 0}, -(1LL << 62)},
    {{2, -(1LL << 61), 0}, -(1LL << 62)},
    {{-(1LL << 61), -2, 0}, 1LL << 62},

    // large negative values with large shifts
    {{-(1LL << 62), 1, 62}, -1},
    {{1LL << 62, -1, 62}, -1},
    {{-(1LL << 62), -1, 62}, 1},

    // boundary
    {{max, 1, 0}, max},
    {{max, 2, 1}, max},
    {{max, -1, 0}, -max},
    {{-max, 1, 0}, -max},
    {{-max, -1, 0}, max},

    // various zeros
    {{0, -100, 5}, 0},
    {{-100, 0, 5}, 0},
    {{0, -(1LL << 62), 32}, 0},
};

INSTANTIATE_TEST_SUITE_P(mul_params, int128_test_mul_i64_i64_shr_t,
                         ValuesIn(mul_params));

// ----------------------------------------------------------------------------
// Division
// ----------------------------------------------------------------------------

struct int128_test_div_i64_i64_shl_t : int128_test_t {};

TEST_P(int128_test_div_i64_i64_shl_t, result) {
  const auto& input = GetParam().input;
  ASSERT_EQ(curves_div_i64_i64_shl(input.left, input.right, input.shift),
            GetParam().expected);
}

const param_t div_params[] = {
    // zero
    {{0, 1, 0}, 0},
    {{0, -1, 0}, 0},

    // simple positive
    {{1, 1, 0}, 1},
    {{1, 1, 1}, 2},

    // numerator < denominator
    {{15, 26, 2}, (15 << 2) / 26},
    {{11, 89, 3}, (11 << 3) / 89},

    // numerator > denominator
    {{26, 15, 2}, (26 << 2) / 15},
    {{89, 11, 3}, (89 << 3) / 11},

    // unity
    {{100, 100, 10}, 1LL << 10},
    {{1000, 1000, 20}, 1LL << 20},

    // fixed point values
    {{1447LL << 32, 13LL << 32, 32}, (1447LL << 32) / 13LL},
    {{13LL << 32, 1447LL << 32, 32}, (13LL << 32) / 1447LL},

    // large positive values
    {{1LL << 61, 1, 1}, 1LL << 62},
    {{1LL << 60, 1, 2}, 1LL << 62},
    {{1LL << 62, 2, 1}, 1LL << 62},
    {{1LL << 62, 4, 2}, 1LL << 62},

    // large shifts
    {{1, 1, 62}, 1LL << 62},
    {{1, 1, 63}, 1LL << 63},
    {{1, 2, 63}, 1LL << 62},
    {{1, 1LL << 10, 63}, 1LL << 53},

    // small numerator / large denominator
    {{1, 1LL << 62, 62}, 1LL},
    {{1, 1LL << 62, 63}, 2},
    {{10, 1LL << 62, 63}, 20},

    // simple negatives
    {{-1, 1, 0}, -1},
    {{1, -1, 0}, -1},
    {{-1, -1, 0}, 1},
    {{-100, 1, 0}, -100},
    {{100, -1, 0}, -100},

    // negative / positive
    {{-15, 26, 2}, (-15 << 2) / 26},
    {{-89, 11, 3}, (-89 << 3) / 11},

    // positive / negative
    {{15, -26, 2}, (15 << 2) / -26},
    {{89, -11, 3}, (89 << 3) / -11},

    // negative / negative
    {{-15, -26, 2}, (-15 << 2) / -26},
    {{-89, -11, 3}, (-89 << 3) / -11},

    // negative unity
    {{-100, -100, 10}, 1LL << 10},
    {{-1000, -1000, 20}, 1LL << 20},
    {{100, -100, 10}, -(1LL << 10)},
    {{-100, 100, 10}, -(1LL << 10)},

    // negative fixed point values
    {{-1447LL << 32, 13LL << 32, 32}, (-1447LL << 32) / 13LL},
    {{1447LL << 32, -13LL << 32, 32}, (1447LL << 32) / -13LL},
    {{-1447LL << 32, -13LL << 32, 32}, (-1447LL << 32) / -13LL},

    // large negative values
    {{-(1LL << 61), 1, 1}, -(1LL << 62)},
    {{-(1LL << 60), 1, 2}, -(1LL << 62)},
    {{1LL << 61, -1, 1}, -(1LL << 62)},

    // negative values with large shifts
    {{-1, 1, 63}, min},
    {{-1, -1, 63}, 1LL << 63},
    {{-1, 1LL << 62, 63}, -2},

    // max boundary
    {{max, 1, 0}, max},
    {{max, -1, 0}, -max},

    // various zeros
    {{0, -100, 10}, 0},
    {{0, -(1LL << 62), 32}, 0},
    {{0, -1, 63}, 0},
};

INSTANTIATE_TEST_SUITE_P(div_params, int128_test_div_i64_i64_shl_t,
                         ValuesIn(div_params));

}  // namespace
}  // namespace curves
