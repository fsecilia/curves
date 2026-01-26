// SPDX-License-Identifier: MIT
/**
  \file
  \copyright Copyright (C) 2026 Frank Secilia
*/

#include <crv/math/fixed.hpp>
#include <crv/test/test.hpp>

namespace crv {
namespace {

using value_t            = int;
constexpr auto frac_bits = 21;
using sut_t              = fixed_t<value_t, frac_bits>;

// zero initialization works
static_assert(sut_t{}.value == 0);

// zero is always zero; there is no offset
static_assert(sut_t{0}.value == 0);

// value initialization is direct; no rescaling is performed
static_assert(sut_t{1}.value == 1);

// 1 is not a special value
static_assert(sut_t{0xF1234}.value == 0xF1234);

} // namespace
} // namespace crv
