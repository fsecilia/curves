// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "algorithm.hpp"
#include <crv/test/test.hpp>
#include <algorithm>
#include <concepts>
#include <limits>

namespace crv {
namespace {

template <typename first_t, typename... remaining_t>
    requires(std::same_as<first_t, remaining_t> && ...)
constexpr auto matches_std(first_t const& first, remaining_t const&... rest) noexcept -> bool
{
    auto const max_matches = (crv::max(first, rest...) == std::max({first, rest...}));
    auto const min_matches = (crv::min(first, rest...) == std::min({first, rest...}));
    return max_matches && min_matches;
}

// --------------------------------------------------------------------------------------------------------------------
// basic permutations and boundaries
// --------------------------------------------------------------------------------------------------------------------

static_assert(matches_std(1, 2));
static_assert(matches_std(2, 1));
static_assert(matches_std(1, 2, 3, 4, 5));
static_assert(matches_std(5, 4, 3, 2, 1));
static_assert(matches_std(1, 5, 2, 4, 3));
static_assert(matches_std(-5, -2, -9, -1));
static_assert(matches_std(0, -1, 1));

// --------------------------------------------------------------------------------------------------------------------
// type deduction and constraints
// --------------------------------------------------------------------------------------------------------------------

static_assert(matches_std(1U, 2U, 3U));
static_assert(matches_std(1L, 2L, 3L));
static_assert(matches_std(1ULL, 2ULL, 3ULL));
static_assert(matches_std(1.5f, 2.5f, 0.5f));
static_assert(matches_std(3.14, 2.71, 1.61));

// --------------------------------------------------------------------------------------------------------------------
// limits
// --------------------------------------------------------------------------------------------------------------------

static_assert(matches_std(std::numeric_limits<int_t>::min(), int_t{0}, std::numeric_limits<int_t>::max()));
static_assert(matches_std(std::numeric_limits<uint_t>::min(), uint_t{0}, std::numeric_limits<uint_t>::max()));

// --------------------------------------------------------------------------------------------------------------------
// non-integer types
// --------------------------------------------------------------------------------------------------------------------

struct custom_t
{
    int_t value;
    constexpr auto operator<=>(custom_t const&) const noexcept -> auto = default;
    constexpr auto operator==(custom_t const&) const noexcept -> bool = default;
};
static_assert(matches_std(custom_t{1}, custom_t{3}, custom_t{2}));

// --------------------------------------------------------------------------------------------------------------------
// address stability; strict weak ordering
// --------------------------------------------------------------------------------------------------------------------

constexpr auto addresses_are_stable() noexcept -> bool
{
    int_t arr[] = {37, 37, 37};

    auto const max_stable = (&crv::max(arr[0], arr[1], arr[2]) == &arr[0]);
    auto const min_stable = (&crv::min(arr[0], arr[1], arr[2]) == &arr[0]);

    return max_stable && min_stable;
}
static_assert(addresses_are_stable());

// --------------------------------------------------------------------------------------------------------------------
// negative compilation tests
// --------------------------------------------------------------------------------------------------------------------

template <typename... args_t>
concept can_call_crv_max = requires(args_t... args) { crv::max(args...); };

// mixed types
static_assert(!can_call_crv_max<int32_t, int64_t>);

// mixed sign
static_assert(!can_call_crv_max<int_t, uint_t>);

// mixed precision
static_assert(!can_call_crv_max<float32_t, float64_t>);

} // namespace
} // namespace crv
