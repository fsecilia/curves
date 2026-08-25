// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \brief kernel module entry point
/// \copyright Copyright (C) 2026 Frank Secilia

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <crv/kernel/control/control.h>
#include <crv/kernel/input/handler.h>
#include <crv/kernel/pipeline/pipeline.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/stringify.h>

/// name reported by input core for the handler and its handles
#define CRV_INPUT_HANDLER_NAME KBUILD_MODNAME

struct crv_input_handle
{
    struct input_handle input;
    struct crv_control_attachment control_attachment;
    unsigned char pipeline_storage[];
};

static struct crv_control crv_control;

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
//     -> publish control attachment
//     -> receive callbacks
//     -> remove control attachment
//     -> close and free handle
//

static bool crv_input_match(struct input_handler* handler, struct input_dev* device)
{
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
    struct crv_input_handle* crv_handle;
    struct input_handle* handle;
    size_t allocation_size;
    int error;

    (void)id;

    pr_info("attaching to input device '%s', phys '%s', bus %04x vendor %04x product %04x version %04x, "
            "input-value capacity %u\n",
        device->name ? device->name : "<unnamed>", device->phys ? device->phys : "<unknown>", device->id.bustype,
        device->id.vendor, device->id.product, device->id.version, device->max_vals);

    allocation_size = sizeof(*crv_handle) + crv_pipeline_storage_size();

    crv_handle = kzalloc(allocation_size, GFP_KERNEL);
    if (!crv_handle)
    {
        error = -ENOMEM;
        goto err_report;
    }

    handle = &crv_handle->input;
    handle->private = crv_pipeline_construct(crv_handle->pipeline_storage);
    handle->dev = device;
    handle->handler = handler;
    handle->name = CRV_INPUT_HANDLER_NAME;

    error = crv_input_register_handle_head(handle);
    if (error) goto err_destroy_pipeline;

    error = input_open_device(handle);
    if (error) goto err_unregister_handle;

    error = crv_control_attach(&crv_control, &crv_handle->control_attachment, handle, handle->private);
    if (error) goto err_close_device;

    pr_info("attached to input device\n");

    return 0;

err_close_device:
    input_close_device(handle);

err_unregister_handle:
    input_unregister_handle(handle);

err_destroy_pipeline:
    crv_pipeline_destroy(handle->private);
    kfree(crv_handle);

err_report:
    pr_err("failed to attach to input device: %d\n", error);
    return error;
}

static noinline __cold void crv_input_report_pipeline_diagnostic(crv_u32_t diagnostic,
    struct crv_pipeline_result_t result, unsigned int count, unsigned int capacity)
{
    switch (diagnostic)
    {
        case CRV_INPUT_PIPELINE_DIAGNOSTIC_SPLIT_REPORT:
            pr_warn_ratelimited(
                "input passed through because Linux split report framing; persistent splits may prevent acceleration "
                "and should be reported as a compatibility issue\n");
            break;

        case CRV_INPUT_PIPELINE_DIAGNOSTIC_INVALID_REPORT:
            pr_warn_ratelimited("malformed input report passed through unchanged\n");
            break;

        case CRV_INPUT_PIPELINE_DIAGNOSTIC_INVALID_COUNT:
            pr_warn_ratelimited("dropping malformed input callback (count %u, capacity %u)\n", count, capacity);
            break;

        case CRV_INPUT_PIPELINE_DIAGNOSTIC_INVALID_TIMESTAMP:
            pr_warn_ratelimited("input timestamp did not increase; report passed through unchanged\n");
            break;

        case CRV_INPUT_PIPELINE_DIAGNOSTIC_VELOCITY_OUT_OF_RANGE:
            pr_warn_ratelimited("input velocity exceeded runtime range; report passed through unchanged\n");
            break;

        case CRV_INPUT_PIPELINE_DIAGNOSTIC_TRANSFORM_INPUT_OUT_OF_RANGE:
            pr_warn_ratelimited("output transform input exceeded runtime range; report passed through unchanged\n");
            break;

        case CRV_INPUT_PIPELINE_DIAGNOSTIC_OUTPUT_OUT_OF_RANGE:
            pr_warn_ratelimited("transformed output exceeded runtime range; report passed through unchanged\n");
            break;

        case CRV_INPUT_PIPELINE_DIAGNOSTIC_APPEND_FAILED:
            pr_warn_ratelimited("input report had no room for transformed axes; report passed through unchanged\n");
            break;

        case CRV_INPUT_PIPELINE_DIAGNOSTIC_UNKNOWN_STATUS:
            WARN_ON_ONCE(1);
            pr_err_ratelimited("pipeline returned unknown status %u\n", result.status);
            break;

        case CRV_INPUT_PIPELINE_DIAGNOSTIC_IMPOSSIBLE_COUNT:
            WARN_ON_ONCE(1);
            pr_err_ratelimited("pipeline returned impossible count %u for capacity %u; dropping callback\n",
                result.count, capacity);
            break;

        case CRV_INPUT_PIPELINE_DIAGNOSTIC_NONE: break;
        default: WARN_ON_ONCE(1); break;
    }
}

static unsigned int crv_input_events(struct input_handle* handle, struct input_value* values, unsigned int count)
{
    struct input_dev* device;
    struct crv_input_pipeline_decision_t decision;
    struct crv_pipeline_result_t result;
    u64 timestamp;

    if (WARN_ON_ONCE(!handle || !values)) return count;

    device = handle->dev;
    if (WARN_ON_ONCE(!device || !handle->private)) return count;

    timestamp = (u64)ktime_to_ns(input_get_timestamp(device)[INPUT_CLK_MONO]);
    result = crv_pipeline_process(handle->private, values, count, device->max_vals, device->num_vals, timestamp);
    decision = crv_input_decide_pipeline_result(result, count, device->max_vals);

    if (decision.diagnostic != CRV_INPUT_PIPELINE_DIAGNOSTIC_NONE)
        crv_input_report_pipeline_diagnostic(decision.diagnostic, result, count, device->max_vals);

    return decision.count;
}

static void crv_input_disconnect(struct input_handle* handle)
{
    struct crv_input_handle* crv_handle = container_of(handle, struct crv_input_handle, input);
    struct input_dev* device = handle->dev;

    crv_control_detach(&crv_control, &crv_handle->control_attachment);
    input_close_device(handle);
    input_unregister_handle(handle);

    pr_info("detached from input device '%s', phys '%s'\n", device->name ? device->name : "<unnamed>",
        device->phys ? device->phys : "<unknown>");

    crv_pipeline_destroy(handle->private);
    kfree(crv_handle);
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

    error = crv_control_register(&crv_control);
    if (error) return error;

    error = input_register_handler(&crv_input_handler);
    if (error)
    {
        pr_err("failed to register input handler: %d\n", error);
        crv_control_unregister(&crv_control);
        return error;
    }

    pr_info("loaded\n");
    return 0;
}

static void __exit crv_exit(void)
{
    input_unregister_handler(&crv_input_handler);
    crv_control_unregister(&crv_control);

    pr_info("unloaded\n");
}

module_init(crv_init);
module_exit(crv_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Frank Secilia");
MODULE_DESCRIPTION("Curves Mouse Acceleration Input Handler");
MODULE_VERSION(__stringify(CRV_VERSION));
