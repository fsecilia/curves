// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "measured_error.hpp"
#include <crv/test/test.hpp>

namespace crv {
namespace {

using measured_error_t = measured_error_t<float_t>;
constexpr auto lesser  = measured_error_t{.position = 0.0, .magnitude = 1.0};
constexpr auto greater = measured_error_t{.position = 0.0, .magnitude = 2.0};

constexpr auto pred = measured_error_pred_t{};
static_assert(!pred(lesser, lesser));
static_assert(pred(lesser, greater));
static_assert(!pred(greater, lesser));
static_assert(!pred(greater, greater));

} // namespace
} // namespace crv
