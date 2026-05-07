// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "interval.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::spline {
namespace {

using real_t = float_t;

struct segment_t
{};
using sut_t = interval_t<real_t, segment_t>;

constexpr auto defects = segment_defects_t{1};
constexpr auto no_defects = segment_defects_t{0};

constexpr auto construct_sut(segment_defects_t segment_defects, real_t weighted_error, real_t left_x) noexcept -> sut_t
{
    auto result = sut_t{};
    result.left.x = left_x;
    result.residual.weighted_error = weighted_error;
    result.segment_defects = segment_defects;
    return result;
}

static_assert(construct_sut(no_defects, 0, 0) <=> construct_sut(no_defects, 0, 0) == std::partial_ordering::equivalent);
static_assert(construct_sut(no_defects, 0, 0) <=> construct_sut(no_defects, 0, 1) == std::partial_ordering::less);
static_assert(construct_sut(no_defects, 0, 0) <=> construct_sut(no_defects, 1, 0) == std::partial_ordering::less);
static_assert(construct_sut(no_defects, 0, 0) <=> construct_sut(no_defects, 1, 1) == std::partial_ordering::less);
static_assert(construct_sut(no_defects, 0, 1) <=> construct_sut(no_defects, 0, 0) == std::partial_ordering::greater);
static_assert(construct_sut(no_defects, 0, 1) <=> construct_sut(no_defects, 0, 1) == std::partial_ordering::equivalent);
static_assert(construct_sut(no_defects, 0, 1) <=> construct_sut(no_defects, 1, 0) == std::partial_ordering::less);
static_assert(construct_sut(no_defects, 0, 1) <=> construct_sut(no_defects, 1, 1) == std::partial_ordering::less);
static_assert(construct_sut(no_defects, 1, 0) <=> construct_sut(no_defects, 0, 0) == std::partial_ordering::greater);
static_assert(construct_sut(no_defects, 1, 0) <=> construct_sut(no_defects, 0, 1) == std::partial_ordering::greater);
static_assert(construct_sut(no_defects, 1, 0) <=> construct_sut(no_defects, 1, 0) == std::partial_ordering::equivalent);
static_assert(construct_sut(no_defects, 1, 0) <=> construct_sut(no_defects, 1, 1) == std::partial_ordering::less);
static_assert(construct_sut(no_defects, 1, 1) <=> construct_sut(no_defects, 0, 0) == std::partial_ordering::greater);
static_assert(construct_sut(no_defects, 1, 1) <=> construct_sut(no_defects, 0, 1) == std::partial_ordering::greater);
static_assert(construct_sut(no_defects, 1, 1) <=> construct_sut(no_defects, 1, 0) == std::partial_ordering::greater);
static_assert(construct_sut(no_defects, 1, 1) <=> construct_sut(no_defects, 1, 1) == std::partial_ordering::equivalent);

static_assert(construct_sut(no_defects, 0, 0) <=> construct_sut(defects, 0, 0) == std::partial_ordering::less);
static_assert(construct_sut(no_defects, 0, 0) <=> construct_sut(defects, 0, 1) == std::partial_ordering::less);
static_assert(construct_sut(no_defects, 0, 0) <=> construct_sut(defects, 1, 0) == std::partial_ordering::less);
static_assert(construct_sut(no_defects, 0, 0) <=> construct_sut(defects, 1, 1) == std::partial_ordering::less);
static_assert(construct_sut(no_defects, 0, 1) <=> construct_sut(defects, 0, 0) == std::partial_ordering::less);
static_assert(construct_sut(no_defects, 0, 1) <=> construct_sut(defects, 0, 1) == std::partial_ordering::less);
static_assert(construct_sut(no_defects, 0, 1) <=> construct_sut(defects, 1, 0) == std::partial_ordering::less);
static_assert(construct_sut(no_defects, 0, 1) <=> construct_sut(defects, 1, 1) == std::partial_ordering::less);
static_assert(construct_sut(no_defects, 1, 0) <=> construct_sut(defects, 0, 0) == std::partial_ordering::less);
static_assert(construct_sut(no_defects, 1, 0) <=> construct_sut(defects, 0, 1) == std::partial_ordering::less);
static_assert(construct_sut(no_defects, 1, 0) <=> construct_sut(defects, 1, 0) == std::partial_ordering::less);
static_assert(construct_sut(no_defects, 1, 0) <=> construct_sut(defects, 1, 1) == std::partial_ordering::less);
static_assert(construct_sut(no_defects, 1, 1) <=> construct_sut(defects, 0, 0) == std::partial_ordering::less);
static_assert(construct_sut(no_defects, 1, 1) <=> construct_sut(defects, 0, 1) == std::partial_ordering::less);
static_assert(construct_sut(no_defects, 1, 1) <=> construct_sut(defects, 1, 0) == std::partial_ordering::less);
static_assert(construct_sut(no_defects, 1, 1) <=> construct_sut(defects, 1, 1) == std::partial_ordering::less);

static_assert(construct_sut(defects, 0, 0) <=> construct_sut(no_defects, 0, 0) == std::partial_ordering::greater);
static_assert(construct_sut(defects, 0, 0) <=> construct_sut(no_defects, 0, 1) == std::partial_ordering::greater);
static_assert(construct_sut(defects, 0, 0) <=> construct_sut(no_defects, 1, 0) == std::partial_ordering::greater);
static_assert(construct_sut(defects, 0, 0) <=> construct_sut(no_defects, 1, 1) == std::partial_ordering::greater);
static_assert(construct_sut(defects, 0, 1) <=> construct_sut(no_defects, 0, 0) == std::partial_ordering::greater);
static_assert(construct_sut(defects, 0, 1) <=> construct_sut(no_defects, 0, 1) == std::partial_ordering::greater);
static_assert(construct_sut(defects, 0, 1) <=> construct_sut(no_defects, 1, 0) == std::partial_ordering::greater);
static_assert(construct_sut(defects, 0, 1) <=> construct_sut(no_defects, 1, 1) == std::partial_ordering::greater);
static_assert(construct_sut(defects, 1, 0) <=> construct_sut(no_defects, 0, 0) == std::partial_ordering::greater);
static_assert(construct_sut(defects, 1, 0) <=> construct_sut(no_defects, 0, 1) == std::partial_ordering::greater);
static_assert(construct_sut(defects, 1, 0) <=> construct_sut(no_defects, 1, 0) == std::partial_ordering::greater);
static_assert(construct_sut(defects, 1, 0) <=> construct_sut(no_defects, 1, 1) == std::partial_ordering::greater);
static_assert(construct_sut(defects, 1, 1) <=> construct_sut(no_defects, 0, 0) == std::partial_ordering::greater);
static_assert(construct_sut(defects, 1, 1) <=> construct_sut(no_defects, 0, 1) == std::partial_ordering::greater);
static_assert(construct_sut(defects, 1, 1) <=> construct_sut(no_defects, 1, 0) == std::partial_ordering::greater);
static_assert(construct_sut(defects, 1, 1) <=> construct_sut(no_defects, 1, 1) == std::partial_ordering::greater);

static_assert(construct_sut(defects, 0, 0) <=> construct_sut(defects, 0, 0) == std::partial_ordering::equivalent);
static_assert(construct_sut(defects, 0, 0) <=> construct_sut(defects, 0, 1) == std::partial_ordering::less);
static_assert(construct_sut(defects, 0, 0) <=> construct_sut(defects, 1, 0) == std::partial_ordering::less);
static_assert(construct_sut(defects, 0, 0) <=> construct_sut(defects, 1, 1) == std::partial_ordering::less);
static_assert(construct_sut(defects, 0, 1) <=> construct_sut(defects, 0, 0) == std::partial_ordering::greater);
static_assert(construct_sut(defects, 0, 1) <=> construct_sut(defects, 0, 1) == std::partial_ordering::equivalent);
static_assert(construct_sut(defects, 0, 1) <=> construct_sut(defects, 1, 0) == std::partial_ordering::less);
static_assert(construct_sut(defects, 0, 1) <=> construct_sut(defects, 1, 1) == std::partial_ordering::less);
static_assert(construct_sut(defects, 1, 0) <=> construct_sut(defects, 0, 0) == std::partial_ordering::greater);
static_assert(construct_sut(defects, 1, 0) <=> construct_sut(defects, 0, 1) == std::partial_ordering::greater);
static_assert(construct_sut(defects, 1, 0) <=> construct_sut(defects, 1, 0) == std::partial_ordering::equivalent);
static_assert(construct_sut(defects, 1, 0) <=> construct_sut(defects, 1, 1) == std::partial_ordering::less);
static_assert(construct_sut(defects, 1, 1) <=> construct_sut(defects, 0, 0) == std::partial_ordering::greater);
static_assert(construct_sut(defects, 1, 1) <=> construct_sut(defects, 0, 1) == std::partial_ordering::greater);
static_assert(construct_sut(defects, 1, 1) <=> construct_sut(defects, 1, 0) == std::partial_ordering::greater);
static_assert(construct_sut(defects, 1, 1) <=> construct_sut(defects, 1, 1) == std::partial_ordering::equivalent);

} // namespace
} // namespace crv::spline
