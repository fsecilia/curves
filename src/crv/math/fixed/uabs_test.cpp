// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "uabs.hpp"
#include <crv/math/fixed/io.hpp>
#include <crv/test/test.hpp>

namespace crv {
namespace {

using int_value_t = int16_t;
using wide_int_value_t = int32_t;
using uint_value_t = make_unsigned_t<int_value_t>;

//
// signed tests
//

using signed_t = fixed_t<int_value_t, 16>;

// return type is unsigned relative to underlying with original frac_bits
static_assert(std::is_same_v<decltype(uabs(min<signed_t>())), fixed_t<uint_value_t, 16>>);

// return values are abs, with special care taken to not induce UB in the tests themselves
static_assert(uabs(min<signed_t>()).value == uint_value_t{-wide_int_value_t(min<signed_t>().value)});
static_assert(uabs(signed_t::literal(-1)).value == 1U);
static_assert(uabs(signed_t::literal(0)).value == 0U);
static_assert(uabs(signed_t::literal(1)).value == 1U);
static_assert(uabs(max<signed_t>()).value == uint_value_t{max<int_value_t>()});

//
// unsigned tests
//

using unsigned_t = fixed_t<uint_value_t, 16>;

// return type is identity
static_assert(std::is_same_v<decltype(uabs(min<unsigned_t>())), unsigned_t>);

// return values are identity
static_assert(uabs(unsigned_t::literal(0)).value == 0U);
static_assert(uabs(unsigned_t::literal(1)).value == 1U);
static_assert(uabs(max<unsigned_t>()).value == uint_value_t{max<uint_value_t>()});

} // namespace
} // namespace crv
