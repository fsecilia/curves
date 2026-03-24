// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "concepts.hpp"
#include <crv/test/test.hpp>

namespace crv::division {
namespace {

using narrow_t = uint32_t;
using wide_t   = uint64_t;

struct arbitrary_t
{};

struct rounding_mode_t
{
    constexpr auto bias(wide_t dividend, narrow_t divisor) const noexcept -> wide_t;
    constexpr auto carry(wide_t quotient, narrow_t divisor, narrow_t remainder) const noexcept -> wide_t;
};

// --------------------------------------------------------------------------------------------------------------------
// is_result
// --------------------------------------------------------------------------------------------------------------------

namespace is_result_test {

struct valid_t
{
    wide_t   quotient;
    narrow_t remainder;
};

struct reversed_sizes_t
{
    narrow_t quotient;
    wide_t   remainder;
};

struct signed_types_t
{
    int64_t quotient;
    int32_t remainder;
};

struct missing_remainder_t
{
    wide_t quotient;
};

struct missing_quotient_t
{
    narrow_t remainder;
};

struct nonadjacent_sizes_t
{
    uint64_t quotient;
    uint8_t  remainder;
};

static_assert(is_result<valid_t>);
static_assert(is_result<valid_t const>);
static_assert(is_result<valid_t&>);
static_assert(is_result<valid_t const&>);
static_assert(is_result<valid_t volatile&&>);

static_assert(is_result<reversed_sizes_t>);
static_assert(is_result<nonadjacent_sizes_t>);

static_assert(!is_result<signed_types_t>);
static_assert(!is_result<missing_quotient_t>);
static_assert(!is_result<missing_remainder_t>);

} // namespace is_result_test

// --------------------------------------------------------------------------------------------------------------------
// is_hardware_divider
// --------------------------------------------------------------------------------------------------------------------

namespace is_hardware_divider_test {

template <unsigned_integral narrow_t> struct valid_hardware_divider_t
{
    using wide_t = wider_t<narrow_t>;
    constexpr auto operator()(wide_t, narrow_t) const noexcept -> result_t<narrow_t>;
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
