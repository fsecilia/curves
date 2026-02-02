// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*!
    \file
    \brief

    \copyright Copyright (C) 2026 Frank Secilia
*/

extern "C" {
#include "cxx_build_test.h"
} // extern "C"

#include <crv/math/fixed/fixed.hpp>

namespace crv {

using q47_16i_t = fixed_t<int, 16>;
using q63_0i_t  = fixed_t<int64_t, 0>;

template class fixed_t<int, 16>;
template class fixed_t<int64_t, 0>;

} // namespace crv

extern "C" int reference_cxx(void)
{
    using namespace crv;

    auto const one = q63_0i_t{1};
    return q47_16i_t{one}.value;
}
