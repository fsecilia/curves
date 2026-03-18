// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "concepts.hpp"
#include <crv/test/test.hpp>

namespace crv::division {
namespace {

struct arbitrary_t
{};

// --------------------------------------------------------------------------------------------------------------------
// is_result
// --------------------------------------------------------------------------------------------------------------------

static_assert(is_result<result_t<int_t>>);
static_assert(is_result<result_t<uint_t>>);

static_assert(is_result<result_t<int_t, int_t>>);
static_assert(is_result<result_t<int_t, uint_t>>);
static_assert(is_result<result_t<uint_t, int_t>>);
static_assert(is_result<result_t<uint_t, uint_t>>);

static_assert(!is_result<arbitrary_t>);

// --------------------------------------------------------------------------------------------------------------------
// is_divider
// --------------------------------------------------------------------------------------------------------------------

template <typename dividend_t, typename divisor_t> struct divider_t
{
    constexpr auto operator()(dividend_t, divisor_t) const noexcept -> result_t<dividend_t, divisor_t> { return {}; }
};

static_assert(is_divider<divider_t<int_t, int_t>, int_t, int_t>);
static_assert(is_divider<divider_t<int_t, uint_t>, int_t, uint_t>);
static_assert(is_divider<divider_t<uint_t, int_t>, uint_t, int_t>);
static_assert(is_divider<divider_t<uint_t, uint_t>, uint_t, uint_t>);

static_assert(!is_divider<divider_t<int_t, int_t>, arbitrary_t, int_t>);
static_assert(!is_divider<divider_t<int_t, int_t>, int_t, arbitrary_t>);

static_assert(!is_divider<arbitrary_t, int_t, int_t>);

// --------------------------------------------------------------------------------------------------------------------
// is_wide_divider
// --------------------------------------------------------------------------------------------------------------------

template <typename narrow_t> struct wide_divider_t
{
    using wide_t = wider_t<narrow_t>;
    constexpr auto operator()(wide_t, narrow_t) const noexcept -> result_t<wide_t, narrow_t> { return {}; }
};

static_assert(is_wide_divider<wide_divider_t<int_t>, int_t>);
static_assert(is_wide_divider<wide_divider_t<uint_t>, uint_t>);

static_assert(!is_wide_divider<wide_divider_t<int_t>, arbitrary_t>);

static_assert(!is_wide_divider<arbitrary_t, int_t>);

} // namespace
} // namespace crv::division
