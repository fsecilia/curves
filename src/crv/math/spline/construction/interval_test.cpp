// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "interval.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::spline {
namespace {

using interval_t = interval_t<float_t, int_t>;
constexpr auto lesser
    = interval_t{.left = 0.0, .right = 0.0, .max_error = 1.0, .tolerance = 0.0, .depth = 0, .approximant = 0};
constexpr auto greater
    = interval_t{.left = 0.0, .right = 0.0, .max_error = 2.0, .tolerance = 0.0, .depth = 0, .approximant = 0};

constexpr auto pred = interval_pred_t{};
static_assert(!pred(lesser, lesser));
static_assert(pred(lesser, greater));
static_assert(!pred(greater, lesser));
static_assert(!pred(greater, greater));

} // namespace
} // namespace crv::spline
