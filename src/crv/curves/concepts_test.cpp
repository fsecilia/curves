// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "concepts.hpp"
#include <stdexcept>

namespace crv {
namespace {

using real_t = float_t;

//
// is_curve
//

struct valid_curve_t
{
    [[nodiscard]] constexpr auto operator()(real_t x) const noexcept -> real_t { return x; }
};

// misses noexcept
struct throwing_curve_t
{
    [[nodiscard, noreturn]] constexpr auto operator()(real_t) const -> real_t { throw std::logic_error{""}; }
};

// returns void instead of real_t
struct wrong_return_curve_t
{
    constexpr auto operator()(real_t) const noexcept -> void {}
};

// takes two arguments
struct wrong_arity_curve_t
{
    [[nodiscard]] constexpr auto operator()(real_t x, real_t) const noexcept -> real_t { return x; }
};

static_assert(is_curve<valid_curve_t, real_t>);
static_assert(!is_curve<throwing_curve_t, real_t>);
static_assert(!is_curve<wrong_return_curve_t, real_t>);
static_assert(!is_curve<wrong_arity_curve_t, real_t>);

} // namespace
} // namespace crv
