// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \brief kernel module entry point
/// \copyright Copyright (C) 2026 Frank Secilia

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/stringify.h>

/// name reported by input core for the handler and its handles
#define CRV_INPUT_HANDLER_NAME KBUILD_MODNAME

// input core applies this capability table then calls match() for policy checks
static const struct input_device_id crv_input_device_ids[] = {
    {
        .flags = INPUT_DEVICE_ID_MATCH_EVBIT | INPUT_DEVICE_ID_MATCH_KEYBIT | INPUT_DEVICE_ID_MATCH_RELBIT,
        .evbit = {[BIT_WORD(EV_KEY)] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REL)},
        .keybit = {[BIT_WORD(BTN_LEFT)] = BIT_MASK(BTN_LEFT)},
        .relbit = {[BIT_WORD(REL_X)] = BIT_MASK(REL_X) | BIT_MASK(REL_Y)},
    },
    {},
};

MODULE_DEVICE_TABLE(input, crv_input_device_ids);

/// implements input_register_handle, but inserts to head instead of tail
static int crv_input_register_handle_head(struct input_handle* handle)
{
    struct input_handler* handler = handle->handler;
    struct input_dev* dev = handle->dev;
    int error;

    handle->handle_events = handler->events;

    error = mutex_lock_interruptible(&dev->mutex);
    if (error) return error;
    list_add_rcu(&handle->d_node, &dev->h_list);
    mutex_unlock(&dev->mutex);

    list_add_tail_rcu(&handle->h_node, &handler->h_list);

    if (handler->start) handler->start(handle);

    return 0;
}

//
// input-device lifecycle
//
// capability match
//     -> policy match
//     -> allocate and register handle
//     -> open device
//     -> attach capture observer
//     -> receive callbacks
//     -> detach capture observer
//     -> close and free handle
//

static bool crv_input_match(struct input_handler* handler, struct input_dev* device)
{
    // first eligible device wins

    (void)handler;

    // accepts external mice and virtual relative-pointer streams
    switch (device->id.bustype)
    {
        case BUS_USB:
        case BUS_BLUETOOTH:
        case BUS_VIRTUAL: break;

        default: return false;
    }

    // reject device classes that may expose mouse-like capabilities but are not relative mice
    if (test_bit(INPUT_PROP_DIRECT, device->propbit) || test_bit(INPUT_PROP_BUTTONPAD, device->propbit)
        || test_bit(INPUT_PROP_SEMI_MT, device->propbit) || test_bit(INPUT_PROP_TOPBUTTONPAD, device->propbit)
        || test_bit(INPUT_PROP_POINTING_STICK, device->propbit) || test_bit(INPUT_PROP_ACCELEROMETER, device->propbit))
    {
        return false;
    }

#ifdef INPUT_PROP_PRESSUREPAD
    if (test_bit(INPUT_PROP_PRESSUREPAD, device->propbit)) return false;
#endif

    // filter touch and tablet nodes whose property bits are absent or incomplete
    if (test_bit(BTN_TOUCH, device->keybit) || test_bit(BTN_TOOL_FINGER, device->keybit)
        || test_bit(BTN_TOOL_PEN, device->keybit) || test_bit(BTN_TOOL_MOUSE, device->keybit))
    {
        return false;
    }

    return true;
}

static int crv_input_connect(struct input_handler* handler, struct input_dev* device, const struct input_device_id* id)
{
    struct input_handle* handle;
    int error;

    (void)id;

    pr_info("attaching to input device '%s', phys '%s', bus %04x vendor %04x product %04x version %04x, "
            "input-value capacity %u\n",
        device->name ? device->name : "<unnamed>", device->phys ? device->phys : "<unknown>", device->id.bustype,
        device->id.vendor, device->id.product, device->id.version, device->max_vals);

    handle = kzalloc(sizeof(*handle), GFP_KERNEL);
    if (!handle)
    {
        error = -ENOMEM;
        goto err_report;
    }

    handle->dev = device;
    handle->handler = handler;
    handle->name = CRV_INPUT_HANDLER_NAME;

    error = crv_input_register_handle_head(handle);
    if (error) goto err_free_handle;

    error = input_open_device(handle);
    if (error) goto err_unregister_handle;

    pr_info("attached to input device\n");

    return 0;

err_unregister_handle:
    input_unregister_handle(handle);

err_free_handle:
    kfree(handle);

err_report:
    pr_err("failed to attach to input device: %d\n", error);
    return error;
}

static unsigned int crv_input_events(struct input_handle* handle, struct input_value* values, unsigned int count)
{
    if (!count) return 0;
    if (WARN_ON_ONCE(!handle || !values)) return count;

    return count;
}

static void crv_input_disconnect(struct input_handle* handle)
{
    struct input_dev* device = handle->dev;

    input_close_device(handle);
    input_unregister_handle(handle);

    pr_info("detached from input device '%s', phys '%s'\n", device->name ? device->name : "<unnamed>",
        device->phys ? device->phys : "<unknown>");

    kfree(handle);
}

static struct input_handler crv_input_handler = {
    .events = crv_input_events,
    .connect = crv_input_connect,
    .disconnect = crv_input_disconnect,
    .match = crv_input_match,
    .name = CRV_INPUT_HANDLER_NAME,
    .id_table = crv_input_device_ids,
};

static int __init crv_init(void)
{
    int error;

    error = input_register_handler(&crv_input_handler);
    if (error)
    {
        pr_err("failed to register input handler: %d\n", error);
        return error;
    }

    pr_info("loaded\n");
    return 0;
}

static void __exit crv_exit(void)
{
    input_unregister_handler(&crv_input_handler);

    pr_info("unloaded\n");
}

module_init(crv_init);
module_exit(crv_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Frank Secilia");
MODULE_DESCRIPTION("Curves Mouse Acceleration Input Handler");
MODULE_VERSION(__stringify(CRV_VERSION));
