// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "cmp.hpp"
#include <crv/lib.hpp>
#include <crv/test/test.hpp>

namespace crv {
namespace {

// same-sign comparisons
static_assert(cmp_equal(int32_t{-5}, int32_t{-5}));
static_assert(!cmp_equal(int32_t{-5}, int32_t{5}));
static_assert(cmp_less(int32_t{-10}, int32_t{-5}));
static_assert(cmp_greater(uint32_t{10}, uint32_t{5}));

// standard mixed-sign comparisons
static_assert(cmp_less(int32_t{-1}, uint32_t{1}));
static_assert(cmp_less(int8_t{-1}, uint64_t{1}));
static_assert(cmp_greater(uint32_t{1}, int32_t{-1}));
static_assert(cmp_equal(int32_t{5}, uint32_t{5}));
static_assert(cmp_equal(uint32_t{5}, int32_t{5}));
static_assert(!cmp_equal(int32_t{-5}, uint32_t{5}));

// 128-bit mixed-sign comparisons
static_assert(cmp_less(int128_t{-1}, uint128_t{1}));
static_assert(cmp_greater(uint128_t{1}, int128_t{-1}));
static_assert(cmp_equal(int128_t{100}, uint128_t{100}));
static_assert(!cmp_equal(int128_t{-100}, uint128_t{100}));

// 128-bit vs standard types
static_assert(cmp_less(int128_t{-5000}, uint32_t{5}));
static_assert(cmp_greater(uint128_t{5000}, int32_t{-5}));
static_assert(cmp_equal(int128_t{100}, uint8_t{100}));

// Edge cases with 0
static_assert(cmp_equal(int32_t{0}, uint32_t{0}));
static_assert(cmp_less_equal(int32_t{0}, uint32_t{0}));
static_assert(cmp_greater_equal(int32_t{0}, uint32_t{0}));

} // namespace
} // namespace crv
