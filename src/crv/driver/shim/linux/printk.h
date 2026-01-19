// SPDX-License-Identifier: MIT
/**
 * Copyright (C) 2026 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#pragma once

#include <stdio.h>

#define printk(fmt, ...) printf(fmt, ##VA_ARGS)
