// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "concepts.hpp"
#include <crv/test/test.hpp>

namespace crv::division {
namespace {

using narrow_t = uint32_t;
using wide_t = uint64_t;

struct arbitrary_t
{};

struct rounding_mode_t
{
    constexpr auto bias(wide_t dividend, narrow_t divisor) const noexcept -> wide_t;
    constexpr auto carry(wide_t quotient, narrow_t divisor, narrow_t remainder) const noexcept -> wide_t;
};

// --------------------------------------------------------------------------------------------------------------------
// is_hardware_divider
// --------------------------------------------------------------------------------------------------------------------

namespace is_hardware_divider_test {

template <unsigned_integral narrow_t> struct valid_hardware_divider_t
{
    using wide_t = widened_t<narrow_t>;
    constexpr auto operator()(wide_t, narrow_t) const noexcept -> qr_pair_t<narrow_t>;
};

static_assert(is_hardware_divider<valid_hardware_divider_t<uint_t>, uint_t>);
static_assert(is_hardware_divider<valid_hardware_divider_t<uint8_t>, uint8_t>);
static_assert(is_hardware_divider<valid_hardware_divider_t<uint32_t>, uint32_t>);
static_assert(!is_hardware_divider<valid_hardware_divider_t<uint_t>, int_t>);
static_assert(!is_hardware_divider<arbitrary_t, uint_t>);

} // namespace is_hardware_divider_test

// ====================================================================================================================
// is_divider
// ====================================================================================================================

namespace is_divider_test {

struct valid_divider_t
{
    constexpr auto operator()(wide_t, narrow_t, rounding_mode_t) const noexcept -> wide_t;
};

static_assert(is_wide_divider<valid_divider_t, narrow_t, rounding_mode_t>);
static_assert(!is_wide_divider<arbitrary_t, narrow_t, rounding_mode_t>);

struct wrong_return_divider_t
{
    constexpr auto operator()(wide_t, narrow_t, rounding_mode_t) const noexcept -> narrow_t;
};
static_assert(!is_wide_divider<wrong_return_divider_t, narrow_t, rounding_mode_t>);

} // namespace is_divider_test

// ====================================================================================================================
// is_narrow_divider
// ====================================================================================================================

namespace is_narrow_divider_test {

struct valid_narrow_divider_t
{
    constexpr auto operator()(narrow_t, narrow_t, rounding_mode_t) const noexcept -> wide_t;
};

static_assert(is_narrow_divider<valid_narrow_divider_t, narrow_t, rounding_mode_t>);
static_assert(!is_narrow_divider<arbitrary_t, narrow_t, rounding_mode_t>);

struct wrong_return_divider_t
{
    constexpr auto operator()(narrow_t, narrow_t, rounding_mode_t) const noexcept -> narrow_t;
};
static_assert(!is_narrow_divider<wrong_return_divider_t, narrow_t, rounding_mode_t>);

} // namespace is_narrow_divider_test

} // namespace
} // namespace crv::division
