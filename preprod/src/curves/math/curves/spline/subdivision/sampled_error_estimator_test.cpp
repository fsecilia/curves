// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2026 Frank Secilia
*/

#include "sampled_error_estimator.hpp"
#include <curves/testing/test.hpp>
#include <gmock/gmock.h>
#include <ostream>
#include <string>
#include <vector>

namespace curves {
namespace {

// ----------------------------------------------------------------------------
// Test Sample
// ----------------------------------------------------------------------------

struct Sample {
  real_t t_candidate;
  real_t y_approximation;
  real_t expected_v_t;
  real_t y_true;

  friend auto operator<<(std::ostream& out, const Sample& src)
      -> std::ostream& {
    return out << "{.t_candidate = " << src.t_candidate
               << ", .y_approximation = " << src.y_approximation
               << "}, .expected_v_t = " << src.expected_v_t
               << ", .y_true = " << src.y_true << "}";
  }
};
using Samples = std::vector<Sample>;

// ----------------------------------------------------------------------------
// Test Vector
// ----------------------------------------------------------------------------

struct TestVector {
  std::string description;
  Samples samples;
  real_t v0;
  real_t segment_width;
  SegmentErrorEstimate expected_result;
  real_t tolerance = 1e-10;

  friend auto operator<<(std::ostream& out, const TestVector& src)
      -> std::ostream& {
    out << "{.description = \"" << src.description << "\", .samples = {";

    auto samples = std::begin(src.samples);
    const auto samples_end = std::end(src.samples);
    if (samples != samples_end) {
      out << *samples++;
      while (samples != samples_end) out << ", " << *samples++;
    }

    out << "}, .v0 = " << src.v0 << "}, .segment_width = " << src.segment_width
        << "}, .expected_result = " << src.expected_result
        << ", .tolerance = " << src.tolerance << "}";

    return out;
  }
};

// ----------------------------------------------------------------------------
// Test Fixture
// ----------------------------------------------------------------------------

struct SegmentErrorEstimatorTest : TestWithParam<TestVector> {
  const Samples& samples = GetParam().samples;
  const real_t segment_width = GetParam().segment_width;
  const real_t v0 = GetParam().v0;
  const SegmentErrorEstimate expected_result = GetParam().expected_result;
  const real_t tolerance = GetParam().tolerance;

  struct MockCurve {
    MOCK_METHOD(real_t, call, (real_t v), (const, noexcept));
    auto operator()(real_t v) const noexcept -> real_t { return call(v); }
    virtual ~MockCurve() = default;
  };
  StrictMock<MockCurve> mock_curve;

  struct MockSegment {
    MOCK_METHOD(real_t, call, (real_t t), (const, noexcept));
    auto operator()(real_t t) const noexcept -> real_t { return call(t); }
    virtual ~MockSegment() = default;
  };
  StrictMock<MockSegment> mock_segment;

  using Candidates = std::vector<real_t>;
  struct MockErrorCandidateLocator {
    MOCK_METHOD(Candidates, call, (const MockSegment&), (const, noexcept));
    auto operator()(const MockSegment& segment) const noexcept -> Candidates {
      return call(segment);
    }
    virtual ~MockErrorCandidateLocator() = default;
  };

  auto create_candidates() const -> Candidates {
    Candidates result;
    for (const auto& sample : samples) result.push_back(sample.t_candidate);
    return result;
  }

  using Sut = SampledErrorEstimator<StrictMock<MockErrorCandidateLocator>>;
  Sut sut;
};

// ----------------------------------------------------------------------------
// Test Case
// ----------------------------------------------------------------------------

TEST_P(SegmentErrorEstimatorTest, Call) {
  EXPECT_CALL(sut.locate_error_candidates, call(Ref(mock_segment)))
      .WillOnce(Return(create_candidates()));

  for (const auto& sample : samples) {
    EXPECT_CALL(mock_segment, call(sample.t_candidate))
        .WillOnce(Return(sample.y_approximation));
    EXPECT_CALL(mock_curve, call(sample.expected_v_t))
        .WillOnce(Return(sample.y_true));
  }

  const auto actual_result = sut(mock_curve, mock_segment, v0, segment_width);

  EXPECT_NEAR(expected_result.v, actual_result.v, tolerance);
  EXPECT_NEAR(expected_result.error, actual_result.error, tolerance);
}

// ----------------------------------------------------------------------------
// Case Builder
// ----------------------------------------------------------------------------

// Small DSL to tame gnarly test vectors.
struct CaseBuilder {
  TestVector result;

  real_t max_err = -1.0;
  real_t best_v = 0.0;

  explicit CaseBuilder(std::string description) {
    result.description = std::move(description);
    with_interval(0.0, 1.0);
  }

  auto with_interval(real_t v0, real_t width) -> CaseBuilder& {
    result.v0 = v0;
    result.segment_width = width;

    // Reset default midpoint expectation in case no samples are added
    best_v = v0 + 0.5 * width;
    max_err = 0.0;

    return *this;
  }

  auto with_candidate(real_t t, real_t y_approx, real_t y_true)
      -> CaseBuilder& {
    const real_t v_t = result.v0 + t * result.segment_width;
    const real_t err = std::abs(y_approx - y_true);

    // Add sample with calculated expected_v_t.
    result.samples.push_back({.t_candidate = t,
                              .y_approximation = y_approx,
                              .expected_v_t = v_t,
                              .y_true = y_true});

    // Update expected winner (argmax).
    if (err > max_err) {
      max_err = err;
      best_v = v_t;
    }

    return *this;
  }

  operator TestVector() const {
    auto final_vector = result;
    final_vector.expected_result = {.v = best_v, .error = max_err};
    return final_vector;
  }
};

// ----------------------------------------------------------------------------
// Test Vectors
// ----------------------------------------------------------------------------

const TestVector test_vectors[] = {
    CaseBuilder("0 samples").with_interval(1.3, 2.2),

    CaseBuilder("1 sample")
        .with_interval(1.7, 2.8)
        .with_candidate(0.15, 3.1, 4.5),

    CaseBuilder("3 samples (middle wins)")
        .with_interval(2.1, 2.5)
        .with_candidate(0.15, 3.1, 4.5)   // error: 1.4
        .with_candidate(0.45, 0.5, 21.2)  // error: 20.7, winner
        .with_candidate(0.95, 3.2, 4.4),  // error: 1.2
};
INSTANTIATE_TEST_SUITE_P(test_vectors, SegmentErrorEstimatorTest,
                         ValuesIn(test_vectors));

}  // namespace
}  // namespace curves
