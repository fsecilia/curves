// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "concepts.hpp"
#include <crv/test/test.hpp>

namespace crv::division {
namespace {

struct arbitrary_t
{};

struct rounding_mode_t
{
    constexpr auto bias(uint64_t dividend, uint32_t divisor) const noexcept -> uint64_t;
    constexpr auto carry(uint64_t quotient, uint32_t divisor, uint32_t remainder) const noexcept -> uint64_t;
};

// --------------------------------------------------------------------------------------------------------------------
// is_result
// --------------------------------------------------------------------------------------------------------------------

namespace is_result_test {

struct valid_t
{
    uint64_t quotient;
    uint32_t remainder;
};

struct reversed_sizes_t
{
    uint32_t quotient;
    uint64_t remainder;
};

struct signed_types_t
{
    int64_t quotient;
    int32_t remainder;
};

struct missing_remainder_t
{
    uint64_t quotient;
};

struct missing_quotient_t
{
    uint32_t remainder;
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
    constexpr auto operator()(uint64_t, uint32_t, rounding_mode_t) const noexcept -> uint64_t;
};

static_assert(is_divider<valid_divider_t, uint32_t, rounding_mode_t>);
static_assert(!is_divider<arbitrary_t, uint32_t, rounding_mode_t>);

struct wrong_return_divider_t
{
    constexpr auto operator()(uint64_t, uint32_t, rounding_mode_t) const noexcept -> uint32_t;
};
static_assert(!is_divider<wrong_return_divider_t, uint32_t, rounding_mode_t>);

} // namespace is_divider_test

// ====================================================================================================================
// is_rounded_divider
// ====================================================================================================================

namespace is_rounded_divider_test {

struct valid_rounded_divider_t
{
    constexpr auto operator()(uint32_t, uint32_t, rounding_mode_t) const noexcept -> uint64_t;
};

static_assert(is_rounded_divider<valid_rounded_divider_t, uint32_t, rounding_mode_t>);
static_assert(!is_rounded_divider<arbitrary_t, uint32_t, rounding_mode_t>);

struct wrong_return_divider_t
{
    constexpr auto operator()(uint32_t, uint32_t, rounding_mode_t) const noexcept -> uint32_t;
};
static_assert(!is_rounded_divider<wrong_return_divider_t, uint32_t, rounding_mode_t>);

} // namespace is_rounded_divider_test

} // namespace
} // namespace crv::division
