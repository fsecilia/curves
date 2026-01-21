// SPDX-License-Identifier: GPL-2.0+ OR MIT

#include "fpu_guard.hpp"

extern "C" {
void fpu_begin(void);
void fpu_end(void);
}

namespace crv {

fpu_guard_t::fpu_guard_t() noexcept
{
    fpu_begin();
}

fpu_guard_t::~fpu_guard_t()
{
    fpu_end();
}

} // namespace crv
