// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \brief kernel module entry point
/// \copyright Copyright (C) 2026 Frank Secilia

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <crv/kernel/control/control.h>
#include <crv/kernel/input/handler.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/stringify.h>

static struct crv_control_t crv_control;
static struct crv_input_handler_t crv_input_handler;

static int __init crv_init(void)
{
    int error;

    error = crv_control_register(&crv_control);
    if (error) return error;

    error = crv_input_handler_register(&crv_input_handler, &crv_control);
    if (error)
    {
        crv_control_unregister(&crv_control);
        return error;
    }

    pr_info("loaded\n");
    return 0;
}

static void __exit crv_exit(void)
{
    crv_input_handler_unregister(&crv_input_handler);
    crv_control_unregister(&crv_control);

    pr_info("unloaded\n");
}

module_init(crv_init);
module_exit(crv_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Frank Secilia");
MODULE_DESCRIPTION("Curves Mouse Acceleration Input Handler");
MODULE_VERSION(__stringify(CRV_VERSION));
