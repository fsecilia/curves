// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*!
    \file
    \brief Curves kernel module entry point.

    \copyright Copyright (C) 2026 Frank Secilia
*/

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/stringify.h>

static int __init crv_init(void)
{
    printk("crv_init\n");
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
