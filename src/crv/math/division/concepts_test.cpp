// SPDX-License-Identifier: MIT
/**
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

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

} // namespace
} // namespace crv::division
