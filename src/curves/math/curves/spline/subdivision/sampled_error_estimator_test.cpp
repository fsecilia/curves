// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include "sampled_error_estimator.hpp"
#include <curves/testing/test.hpp>
#include <cmath>
#include <gmock/gmock.h>
#include <ostream>
#include <string>
#include <vector>

namespace curves {
namespace {

using Scalar = double;
using SegmentErrorEstimate = SegmentErrorEstimate<Scalar>;

struct Sample {
  Scalar t_candidate;
  Scalar y_approximation;
  Scalar expected_v_t;
  Scalar y_true;

  friend auto operator<<(std::ostream& out, const Sample& src)
      -> std::ostream& {
    return out << "{.t_candidate = " << src.t_candidate
               << ", .y_approximation = " << src.y_approximation
               << "}, .expected_v_t = " << src.expected_v_t
               << ", .y_true = " << src.y_true << "}";
  }
};
using Samples = std::vector<Sample>;

struct TestVector {
  std::string description;
  Samples samples;
  Scalar v0;
  Scalar segment_width;
  SegmentErrorEstimate expected_result;
  Scalar tolerance = 1e-10;

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

struct SegmentErrorEstimatorTest : TestWithParam<TestVector> {
  const Samples& samples = GetParam().samples;
  const Scalar segment_width = GetParam().segment_width;
  const Scalar v0 = GetParam().v0;
  const SegmentErrorEstimate expected_result = GetParam().expected_result;
  const Scalar tolerance = GetParam().tolerance;

  struct MockCurve {
    using Scalar = Scalar;
    MOCK_METHOD(Scalar, call, (Scalar v), (const, noexcept));
    auto operator()(Scalar v) const noexcept -> Scalar { return call(v); }
    virtual ~MockCurve() = default;
  };
  StrictMock<MockCurve> mock_curve;

  struct MockSegment {
    MOCK_METHOD(Scalar, call, (Scalar t), (const, noexcept));
    auto operator()(Scalar t) const noexcept -> Scalar { return call(t); }
    virtual ~MockSegment() = default;
  };
  StrictMock<MockSegment> mock_segment;

  using Candidates = std::vector<Scalar>;
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

const TestVector test_vectors[] = {
    {
        .description = "0 samples",
        .samples = {},
        .v0 = 1.3,
        .segment_width = 2.2,
        .expected_result = {.v = 1.3 + 0.5 * 2.2, .error = 0.0},
    },

    {
        .description = "1 sample",
        .samples =
            {
                {
                    .t_candidate = 0.15,
                    .y_approximation = 3.1,
                    .expected_v_t = 1.7 + 0.15 * 2.8,
                    .y_true = 4.5,
                },
            },
        .v0 = 1.7,
        .segment_width = 2.8,
        .expected_result = {.v = 1.7 + 0.15 * 2.8, .error = 4.5 - 3.1},
    },

    {
        .description = "3 samples",
        .samples =
            {
                /*
                  Same slot and value as the sample in 1 sample vector to make
                  sure it isn't being chosen unduly.
                */
                {
                    .t_candidate = 0.15,
                    .y_approximation = 3.1,
                    .expected_v_t = 2.1 + 0.15 * 2.5,
                    .y_true = 4.5,
                },

                // This sample wins because it has the largest error.
                {
                    .t_candidate = 0.45,
                    .y_approximation = 0.5,
                    .expected_v_t = 2.1 + 0.45 * 2.5,
                    .y_true = 21.2,
                },

                {
                    .t_candidate = 0.95,
                    .y_approximation = 3.2,
                    .expected_v_t = 2.1 + 0.95 * 2.5,
                    .y_true = 4.4,
                },
            },
        .v0 = 2.1,
        .segment_width = 2.5,
        .expected_result = {.v = 2.1 + 0.45 * 2.5, .error = 21.2 - 0.5},
    },
};
INSTANTIATE_TEST_SUITE_P(test_vectors, SegmentErrorEstimatorTest,
                         ValuesIn(test_vectors));

}  // namespace
}  // namespace curves
