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

template <typename narrow_t> struct hardware_divider_t
{
    using wide_t = wider_t<narrow_t>;
    constexpr auto operator()(wide_t, narrow_t) const noexcept -> result_t<wide_t, narrow_t> { return {}; }
};

static_assert(!is_hardware_divider<hardware_divider_t<int_t>, int_t>);
static_assert(is_hardware_divider<hardware_divider_t<uint_t>, uint_t>);

static_assert(!is_hardware_divider<hardware_divider_t<int_t>, arbitrary_t>);

static_assert(!is_hardware_divider<arbitrary_t, int_t>);

} // namespace
} // namespace crv::division
