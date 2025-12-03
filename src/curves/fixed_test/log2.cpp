// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include <curves/testing/test.hpp>
#include <curves/fixed.hpp>

namespace curves {
namespace {

struct Log2Param {
  u64 x;
  unsigned int x_frac_bits;
  unsigned int output_frac_bits;
  s64 tolerance;
  s64 expected_result;

  friend auto operator<<(std::ostream& out, const Log2Param& src)
      -> std::ostream& {
    return out << "{" << src.x << ", " << src.x_frac_bits << ", "
               << src.output_frac_bits << ", " << src.tolerance << ", "
               << src.expected_result << "}";
  }
};

struct Log2Test : TestWithParam<Log2Param> {};

TEST_P(Log2Test, expected_result) {
  const auto expected_result = GetParam().expected_result;
  const auto expected_delta = GetParam().tolerance;

  const auto actual_result = curves_fixed_log2(
      GetParam().x, GetParam().x_frac_bits, GetParam().output_frac_bits);

  const auto actual_delta = actual_result > expected_result
                                ? actual_result - expected_result
                                : expected_result - actual_result;
  EXPECT_LE(actual_delta, expected_delta)
      << "Input:     " << GetParam().x << "@Q" << GetParam().x_frac_bits
      << "\nExpected:  " << expected_result << "@Q"
      << GetParam().output_frac_bits << "\nActual:    " << actual_result << "@Q"
      << GetParam().output_frac_bits << "\nDiff:      " << actual_delta
      << "\nTolerance: " << expected_delta;
}

const Log2Param log2_smoke_tests[] = {
    {1ULL << 30, 32, 63, 0, S64_MIN},  // 0.25 saturates to -1.0

    // pure integer parts, no polynomial
    {16, 0, 0, 0, 4},
    {32, 0, 0, 0, 5},
    {64, 0, 0, 0, 6},
    {64, 0, 60, 0, 6LL << 60},

    // pure fractional parts, all polynomial

    // round(0.5*2^16), round(log2(0.5)*2^16)
    {32768, 16, 16, 0, -65536},

    // round(0.5*2^62) , round(log2(0.5)*2^62)
    {2305843009213693952LL, 62, 62, 1, -4611686018427387904LL},

    // integer + with frac parts.
    {65, 0, 16, 0, 394682LL},               // round(log2(65)*2^16)
    {65, 0, 60, 0, 6943317360292612262LL},  // round(log2(65)*2^60)

    // log2(1) = 0 - critical boundary case
    {1ULL << 32, 32, 32, 0, 0},  // 1.0 in Q32.32 format
    {1ULL << 48, 48, 48, 0, 0},  // 1.0 in Q16.48 format

    // Values near 1 (small positive logs)

    // 1.5 -> log2(1.5) ~= 0.585
    {(1ULL << 32) + (1ULL << 31), 32, 32, 0, 2512394810LL},

    // 1.25 -> log2(1.25) ~= 0.322
    {(1ULL << 32) + (1ULL << 30), 32, 32, 0, 1382670639LL},

    // Values near 1 (small negative logs)

    // 0.5 -> log2(0.5) = -1.0
    {(1ULL << 32) - (1ULL << 31), 32, 32, 0, -4294967296LL},

    // 0.75 -> log2(0.75) ~= -0.415
    {(1ULL << 32) - (1ULL << 30), 32, 32, 0, -1782572486LL},

    // Just below partition: sqrt(2) - 1 ~= 0.41421

    // 0.4135 in Q32.32
    {1775063184ULL, 32, 32, 0, -5475124525LL},

    // 0.4156 in Q32.32 (just above)
    {1784375787ULL, 32, 32, 0, -5442701396LL},

    {U64_MAX, 0, 16, 0, 4194304},              // log2(2^64-1) ~= 64.0 in Q48.16
    {U64_MAX >> 1, 0, 32, 0, 270582939648LL},  // log2(2^63-1) ~= 63.0 in Q32.32
    {1ULL << 62, 0, 16, 0, 4063232},           // log2(2^62) = 62.0 in Q48.16

    // High output precision
    {3ULL, 0, 62, 0, 7309349404307464680LL},  // log2(3) ~= 1.585 in Q1.63

    // test 20 vvv

    // Low output precision
    {17ULL, 0, 4, 1, 66},  // log2(17) ~= 4.087 in Q60.4 -> 4*16 + 1 = 65 or 66

    // Asymmetric precisions
    {(65ULL << 50), 50, 16, 0, 394682},  // 65.0 in Q14.50 -> Q48.16
    {65ULL, 4, 32, 0, 8686003617LL},     // 4.0625 in Q60.4 -> Q32.32

    {1, 32, 32, 0, -137438953472LL},  // 2^-32 -> log2 = -32.0 in Q32.32
    {1, 48, 32, 0, -206158430208LL},  // 2^-48 -> log2 = -48.0 in Q32.32

    // Q1.63 output

    // test 25 vvv

    // Within range: log2(0.5) = -1.0 (exactly representable)
    {1ULL << 32, 33, 63, 1, S64_MIN},  // 0.5 in Q31.33 -> -1.0 in Q1.63

    // Within range: log2(1) = 0
    {1ULL << 32, 32, 63, 1, 0},  // 1.0 in Q32.32 -> 0.0 in Q1.63

    // Within range: log2(0.75) ≈ -0.415
    {3ULL << 30, 32, 63, 0, -3828045265094622256LL},  // 0.75 in Q32.32

    // Within range: log2(1.5) ≈ 0.585
    {3ULL << 31, 32, 63, 0, 5395326771760153552LL},  // 1.5 in Q32.32

    // Saturates: log2(2) = 1.0 (would need exactly 2^63, but S64_MAX = 2^63-1)
    {2, 0, 63, 0, S64_MAX},  // 2.0 saturates to ~1.0

    // Saturates: log2(4) = 2.0
    {4, 0, 63, 0, S64_MAX},  // 4.0 saturates to ~1.0

    // Saturates: log2(0.25) = -2.0
    {1ULL << 30, 32, 63, 0, S64_MIN},  // 0.25 saturates to -1.0

    // round(log2(3*2^30/2^32)*2^32)
    {3ULL << 30, 32, 32, 0, -1782572486LL},  // 0.75 in Q32.32

    // round(log2(3*2^31/2^32)*2^32)
    {3ULL << 31, 32, 32, 0, 2512394810LL},  // 1.5 in Q32.32

    // Input designed to maximize accumulated truncation error
    // x = 1.7071... (near sqrt(2), at the partition boundary)
    // This value causes products that consistently lose fractional bits.
    {7318349394477056850ULL, 62, 62, 0, 3072415918868151194LL},
};
INSTANTIATE_TEST_SUITE_P(smoke_tests, Log2Test, ValuesIn(log2_smoke_tests));

}  // namespace
}  // namespace curves
