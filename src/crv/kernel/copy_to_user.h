// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <linux/compiler_types.h>

unsigned long crv_copy_to_user(void __user* dst, void const* src, unsigned long length);
