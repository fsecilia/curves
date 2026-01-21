// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*!
  \file
  \brief wraps kernel_fpu_(begin|end)()

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

namespace crv {

class fpu_guard_t
{
public:
    fpu_guard_t() noexcept;
    ~fpu_guard_t();
};

} // namespace crv
