// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "segment.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv {
namespace {

using mesh_segment_t = mesh_segment_t<float_t, int_t>;
constexpr auto lesser = mesh_segment_t{.left = 0.0, .right = 0.0, .approximant = 0, .max_error = 1.0};
constexpr auto greater = mesh_segment_t{.left = 0.0, .right = 0.0, .approximant = 0, .max_error = 2.0};

constexpr auto pred = mesh_segment_pred_t{};
static_assert(!pred(lesser, lesser));
static_assert(pred(lesser, greater));
static_assert(!pred(greater, lesser));
static_assert(!pred(greater, greater));

} // namespace
} // namespace crv
