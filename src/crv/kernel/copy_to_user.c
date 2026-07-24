// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "copy_to_user.h"
#include <linux/uaccess.h>

unsigned long crv_copy_to_user(void __user* dst, void const* src, unsigned long length)
{
    return copy_to_user(dst, src, length);
}
