// SPDX-License-Identifier: MIT

/// \file
/// \brief project header for all performance executables
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <x86intrin.h>

namespace crv {

/// forces the compiler to compute 'value' without executing additional instructions
template <typename value_t> inline void do_not_optimize(value_t const& value) noexcept
{
    asm volatile("" : : "r,m"(value) : "memory");
}

} // namespace crv
