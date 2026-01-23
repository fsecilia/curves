// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*!
    \file
    \brief Shim header for printk.h.

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <stdio.h>

#define printk(fmt, ...) printf(fmt, ##VA_ARGS)
