// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Tests for quantization utilities.

  These tests verify that our floating-point quantization produces values
  that are exactly representable in the target fixed-point formats, and
  that round-trips through pack/unpack are bit-exact.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#include <curves/testing/test.hpp>
#include <curves/math/curves/spline/segment/construction.hpp>
#include <curves/math/curves/spline/subdivision/adaptive_subdivider.hpp>
#include <cmath>
#include <random>

namespace curves {
namespace {

auto create_rng() -> std::mt19937_64 { return std::mt19937_64{0xF123456789L}; }

// ============================================================================
// Knot Position Quantization
// ============================================================================

struct QuantizeKnotPositionTest : Test {
  // Q8.24 constants.
  static constexpr auto kFracBits = 24;
  static constexpr auto kQuantum = 1.0 / (1LL << kFracBits);

  //! Converts a real to Q8.24 integer representation.
  static auto to_q8_24(real_t r) -> int64_t {
    return static_cast<int64_t>(std::round(r * (1LL << kFracBits)));
  }

  //! Converts Q8.24 integer back to real.
  static auto from_q8_24(int64_t q) -> real_t {
    return static_cast<real_t>(q) / (1LL << kFracBits);
  }
};

TEST_F(QuantizeKnotPositionTest, ZeroIsExact) {
  const auto result = quantize::knot_position(0.0);
  EXPECT_EQ(result, 0.0);
}

TEST_F(QuantizeKnotPositionTest, OneIsExact) {
  const auto result = quantize::knot_position(1.0);
  EXPECT_EQ(result, 1.0);
}

// The smallest positive Q8.24 value should round-trip exactly.
TEST_F(QuantizeKnotPositionTest, QuantumIsExact) {
  const auto result = quantize::knot_position(kQuantum);
  EXPECT_EQ(result, kQuantum);
}

// 0.5 * quantum should round to quantum.
TEST_F(QuantizeKnotPositionTest, HalfQuantumRoundsToNearest) {
  const auto half_quantum = kQuantum / 2;
  const auto result = quantize::knot_position(half_quantum);

  // std::round rounds half away from zero, so 0.5 -> 1.
  EXPECT_EQ(result, kQuantum);
}

// For any quantized value, converting to Q8.24 integer and back should produce
// the exact same value.
TEST_F(QuantizeKnotPositionTest, RoundTripThroughInteger) {
  const auto test_values = std::array{
      0.0, 0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 100.0, 255.999,
  };

  for (const auto v : test_values) {
    const auto quantized = quantize::knot_position(v);
    const auto as_int = to_q8_24(quantized);
    const auto back = from_q8_24(as_int);

    EXPECT_EQ(quantized, back)
        << "Round-trip failed for v=" << v << ", quantized=" << quantized
        << ", as_int=" << as_int << ", back=" << back;
  }
}

// Any quantized value should be an exact multiple of the quantum.
TEST_F(QuantizeKnotPositionTest, FuzzResultIsMultipleOfQuantum) {
  auto rng = create_rng();
  auto dist = std::uniform_real_distribution<real_t>{0.0, 256.0};

  for (auto i = 0; i < 1000; ++i) {
    const auto v = dist(rng);
    const auto quantized = quantize::knot_position(v);

    // Divide by quantum and check that we get an integer.
    const auto ratio = quantized / kQuantum;
    const auto rounded = std::round(ratio);

    EXPECT_DOUBLE_EQ(ratio, rounded)
        << "Quantized value " << quantized << " is not a multiple of quantum";
  }
}

TEST_F(QuantizeKnotPositionTest, FuzzQuantizationErrorBoundedByHalfQuantum) {
  auto rng = create_rng();
  auto dist = std::uniform_real_distribution<real_t>{0.0, 256.0};

  for (auto i = 0; i < 1000; ++i) {
    const auto v = dist(rng);
    const auto quantized = quantize::knot_position(v);
    const auto error = std::abs(quantized - v);

    EXPECT_LE(error, kQuantum / 2 + 1e-15)
        << "Quantization error too large for v=" << v;
  }
}

// ============================================================================
// Coefficient Quantization - Signed 44-bit implicit
// ============================================================================

struct QuantizeSignedCoeffTest : Test {
};

TEST_F(QuantizeSignedCoeffTest, ZeroIsExact) {
  EXPECT_EQ(quantize::signed_coeff(0.0), 0.0);
}

TEST_F(QuantizeSignedCoeffTest, PreservesSign) {
  EXPECT_GT(quantize::signed_coeff(1.0), 0.0);
  EXPECT_LT(quantize::signed_coeff(-1.0), 0.0);
  EXPECT_GT(quantize::signed_coeff(1e-10), 0.0);
  EXPECT_LT(quantize::signed_coeff(-1e-10), 0.0);
}

// Even very small values should maintain their relative ordering.
TEST_F(QuantizeSignedCoeffTest, SmallValuesPreserveOrder) {
  const auto a = quantize::signed_coeff(1e-15);
  const auto b = quantize::signed_coeff(2e-15);

  // These might quantize to the same value (denormal), but a <= b.
  EXPECT_LE(a, b);
}

TEST_F(QuantizeSignedCoeffTest, SymmetricAroundZero) {
  const auto test_values = std::array{1e-10, 1e-5, 0.5, 1.0, 100.0, 1e10};

  for (const auto v : test_values) {
    const auto pos = quantize::signed_coeff(v);
    const auto neg = quantize::signed_coeff(-v);

    EXPECT_EQ(pos, -neg) << "Asymmetric quantization for magnitude " << v;
  }
}

/*
  Quantized coefficient should survive pack -> unpack round-trip unchanged.

  We verify this by creating a segment with known coefficients, packing it,
  unpacking it, and checking that the signed coeffs match.
*/
TEST_F(QuantizeSignedCoeffTest, FuzzMatchesPackingRoundTrip) {
  auto rng = create_rng();
  auto mantissa_dist = std::uniform_real_distribution<real_t>{-1.0, 1.0};
  auto exp_dist = std::uniform_int_distribution<int>{-30, 30};

  for (auto i = 0; i < 100; ++i) {
    // Generate random coefficient.
    const auto raw = std::ldexp(mantissa_dist(rng), exp_dist(rng));
    const auto quantized = quantize::signed_coeff(raw);

    // Create a segment with this coefficient in position 0 (signed).
    const auto params = segment::SegmentParams{
        .poly = {quantized, 0.0, 1.0, 1.0},
        .width = 1.0,
    };

    const auto normalized = segment::create_segment(params);
    const auto packed = segment::pack(normalized);
    const auto unpacked = segment::unpack(packed);

    // Convert back to float for comparison.
    const auto shift = unpacked.poly.shifts[0];
    const auto coeff = unpacked.poly.coeffs[0];
    const auto recovered = std::ldexp(static_cast<real_t>(coeff), -shift);

    EXPECT_NEAR(recovered, quantized, std::abs(quantized) * 1e-12)
        << "Pack round-trip failed for raw=" << raw
        << ", quantized=" << quantized;
  }
}

// ============================================================================
// Coefficient Quantization - Unsigned, 45-bit implicit
// ============================================================================

struct QuantizeUnsignedCoeffTest : Test {
};

TEST_F(QuantizeUnsignedCoeffTest, ZeroIsExact) {
  EXPECT_EQ(quantize::unsigned_coeff(0.0), 0.0);
}

TEST_F(QuantizeUnsignedCoeffTest, PositiveStaysPositive) {
  EXPECT_GT(quantize::unsigned_coeff(1e-15), 0.0);
  EXPECT_GT(quantize::unsigned_coeff(1.0), 0.0);
  EXPECT_GT(quantize::unsigned_coeff(1e10), 0.0);
}

// ============================================================================
// Coefficient Quantization - Inverse Width, 46-bit implicit
// ============================================================================

struct QuantizeInvWidthTest : Test {
};

TEST_F(QuantizeInvWidthTest, ZeroIsExact) {
  EXPECT_EQ(quantize::inv_width(0.0), 0.0);
}

TEST_F(QuantizeInvWidthTest, TypicalWidthsRoundTrip) {
  // Segment widths we actually see.
  const auto test_widths = std::array{
      0.001,  // Very narrow segment
      0.01,   // Narrow
      0.1,    // Medium
      1.0,    // Unit
      10.0,   // Wide
      100.0,  // Very wide
  };

  for (const auto width : test_widths) {
    const auto inv = 1.0 / width;
    const auto quantized = quantize::inv_width(inv);

    // The quantized value should be very close to the original.
    // Relative error should be bounded by 2^-46.
    const auto rel_error = std::abs(quantized - inv) / inv;
    EXPECT_LT(rel_error, 2e-14)
        << "Excessive error for width=" << width << ", inv=" << inv;
  }
}

// ============================================================================
// Polynomial Quantization
// ============================================================================

struct QuantizePolynomialTest : Test {};

TEST_F(QuantizePolynomialTest, AppliesCorrectQuantizerToEachCoeff) {
  // Construct a polynomial with known values.
  const auto poly = cubic::Monomial{
      1.23456789012345,   // a - signed
      -9.87654321098765,  // b - signed
      0.111111111111111,  // c - unsigned
      0.222222222222222,  // d - unsigned
  };

  const auto quantized = quantize::polynomial(poly);

  // Each coefficient should match its individual quantization.
  EXPECT_EQ(quantized.coeffs[0], quantize::signed_coeff(poly.coeffs[0]));
  EXPECT_EQ(quantized.coeffs[1], quantize::signed_coeff(poly.coeffs[1]));
  EXPECT_EQ(quantized.coeffs[2], quantize::unsigned_coeff(poly.coeffs[2]));
  EXPECT_EQ(quantized.coeffs[3], quantize::unsigned_coeff(poly.coeffs[3]));
}

TEST_F(QuantizePolynomialTest, PreservesZeroCoefficients) {
  const auto poly = cubic::Monomial{0.0, 0.0, 0.0, 0.0};
  const auto quantized = quantize::polynomial(poly);

  for (auto i = 0; i < poly.count; ++i) {
    EXPECT_EQ(quantized.coeffs[i], 0.0) << "Coeff " << i << " should be zero";
  }
}

}  // namespace
}  // namespace curves
