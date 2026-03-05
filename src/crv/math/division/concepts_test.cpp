// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "concepts.hpp"
#include <crv/test/test.hpp>

namespace crv::division {
namespace {

// --------------------------------------------------------------------------------------------------------------------
// is_result
// --------------------------------------------------------------------------------------------------------------------

static_assert(is_result<result_t<int_t>>);
static_assert(is_result<result_t<uint_t>>);

static_assert(is_result<result_t<int_t, int_t>>);
static_assert(is_result<result_t<int_t, uint_t>>);
static_assert(is_result<result_t<uint_t, int_t>>);
static_assert(is_result<result_t<uint_t, uint_t>>);

struct not_result_t
{};

static_assert(!is_result<not_result_t>);

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

struct not_convertible_to_int_t
{};

static_assert(!is_divider<divider_t<int_t, int_t>, not_convertible_to_int_t, int_t>);
static_assert(!is_divider<divider_t<int_t, int_t>, int_t, not_convertible_to_int_t>);

struct not_divider_t
{};

static_assert(!is_divider<not_divider_t, int_t, int_t>);

} // namespace
} // namespace crv::division
