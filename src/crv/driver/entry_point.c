// SPDX-License-Identifier: GPL-2.0+ OR MIT
/**
 * Curves kernel module entry point.
 *
 * Copyright (C) 2026 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#include <linux/errno.h>
#include <linux/fpu.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/stringify.h>

static int __init crv_init(void)
{
    printk("crv_init\n");
    if (!kernel_fpu_available()) return -ENOSYS;
    return 0;
}

static void __exit crv_exit(void)
{
    printk("crv_exit\n");
}

module_init(crv_init);
module_exit(crv_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Frank Secilia");
MODULE_DESCRIPTION("Curves Mouse Acceleration Input Handler");
MODULE_VERSION(__stringify(CRV_DRIVER_VERSION));
