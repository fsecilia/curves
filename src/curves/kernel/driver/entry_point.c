// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2025 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#include "entry_point.h"
#include <linux/module.h>
#include <linux/init.h>

static int __init curves_init(void)
{
	return 0;
}

static void __exit curves_exit(void)
{
}

module_init(curves_init);
module_exit(curves_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Frank Secilia");
MODULE_DESCRIPTION("Curves mouse acceleration input handler");
