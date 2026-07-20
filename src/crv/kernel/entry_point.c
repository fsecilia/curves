// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \brief kernel module entry point
/// \copyright Copyright (C) 2026 Frank Secilia

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <crv/kernel/input/capture_abi.h>

#include <linux/atomic.h>
#include <linux/bitops.h>
#include <linux/compiler.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/kfifo.h>
#include <linux/ktime.h>
#include <linux/limits.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/stringify.h>
#include <linux/types.h>
#include <linux/wait.h>

/// number of crv_capture_event objects retained in the stream FIFO
///
/// At 24 bytes per value this occupies 1.5 MiB of module BSS. For a mouse producing four input values at 4 kHz, it
/// holds approximately four seconds.
#define CRV_CAPTURE_FIFO_EVENT_COUNT 65536u

struct crv_capture_source_t
{
    struct input_handle handle;
};

struct crv_capture_state_t
{
    spinlock_t lock;
    wait_queue_head_t read_wait;
    atomic_t opened;

    struct crv_capture_source_t* source;

    bool capture_active;
    bool stream_failed;

    u64 next_batch_sequence;
    u64 dropped_batches;
    u64 dropped_values;
};

static struct crv_capture_state_t crv_capture;

static DECLARE_KFIFO(crv_capture_fifo, struct crv_capture_event_t, CRV_CAPTURE_FIFO_EVENT_COUNT);

static bool crv_capture_read_ready(void)
{
    return !kfifo_is_empty(&crv_capture_fifo) || READ_ONCE(crv_capture.stream_failed)
        || !READ_ONCE(crv_capture.capture_active);
}

static int crv_capture_open(struct inode* inode, struct file* file)
{
    unsigned long flags;
    int error = 0;

    (void)inode;

    if (atomic_cmpxchg(&crv_capture.opened, 0, 1) != 0) return -EBUSY;

    spin_lock_irqsave(&crv_capture.lock, flags);

    if (!crv_capture.source)
    {
        error = -ENODEV;
        goto out_unlock;
    }

    // This occurs under the same lock the producer uses, so no producer can be writing while this reset is performed.
    kfifo_reset(&crv_capture_fifo);

    crv_capture.next_batch_sequence = 0;
    crv_capture.dropped_batches = 0;
    crv_capture.dropped_values = 0;
    crv_capture.stream_failed = false;
    crv_capture.capture_active = true;

    file->private_data = &crv_capture;

out_unlock:
    spin_unlock_irqrestore(&crv_capture.lock, flags);

    if (error) atomic_set(&crv_capture.opened, 0);
    else pr_info("capture stream opened\n");

    return error;
}

static int crv_capture_release(struct inode* inode, struct file* file)
{
    unsigned long flags;
    u64 dropped_batches;
    u64 dropped_values;

    (void)inode;
    (void)file;

    spin_lock_irqsave(&crv_capture.lock, flags);

    crv_capture.capture_active = false;
    dropped_batches = crv_capture.dropped_batches;
    dropped_values = crv_capture.dropped_values;

    spin_unlock_irqrestore(&crv_capture.lock, flags);

    wake_up_interruptible(&crv_capture.read_wait);
    atomic_set(&crv_capture.opened, 0);

    if (dropped_batches)
    {
        pr_warn("capture stream closed after dropping %llu batches (%llu values)\n",
            (unsigned long long)dropped_batches, (unsigned long long)dropped_values);
    }
    else
    {
        pr_info("capture stream closed without drops\n");
    }

    return 0;
}

static __poll_t crv_capture_poll(struct file* file, poll_table* wait)
{
    __poll_t mask = 0;

    poll_wait(file, &crv_capture.read_wait, wait);

    if (!kfifo_is_empty(&crv_capture_fifo)) mask |= EPOLLIN | EPOLLRDNORM;
    if (READ_ONCE(crv_capture.stream_failed)) mask |= EPOLLERR;
    if (!READ_ONCE(crv_capture.capture_active)) mask |= EPOLLHUP;

    return mask;
}

static DEFINE_MUTEX(crv_capture_read_lock);

static ssize_t crv_capture_read(struct file* file, char __user* buffer, size_t count, loff_t* position)
{
    unsigned int copied = 0;
    unsigned int requested;
    ssize_t result;
    int error;

    (void)position;

    if (count == 0) return 0;

    if (count < sizeof(struct crv_capture_event_t)) return -EINVAL;

    requested = count > UINT_MAX ? UINT_MAX : (unsigned int)count;
    requested -= requested % sizeof(struct crv_capture_event_t);

    error = mutex_lock_interruptible(&crv_capture_read_lock);
    if (error) return error;

    for (;;)
    {
        if (!kfifo_is_empty(&crv_capture_fifo)) break;

        /*
            A source disconnect is sticky for the current open session. Buffered values are drained first; the first
            empty read reports it.
        */
        if (READ_ONCE(crv_capture.stream_failed))
        {
            result = -ENODEV;
            goto out_unlock;
        }
        if (!READ_ONCE(crv_capture.capture_active))
        {
            result = -EIO;
            goto out_unlock;
        }
        if (file->f_flags & O_NONBLOCK)
        {
            result = -EAGAIN;
            goto out_unlock;
        }

        error = wait_event_interruptible(crv_capture.read_wait, crv_capture_read_ready());
        if (error)
        {
            result = error;
            goto out_unlock;
        }
    }

    error = kfifo_to_user(&crv_capture_fifo, buffer, requested, &copied);
    result = copied ? (ssize_t)copied : error;

out_unlock:
    mutex_unlock(&crv_capture_read_lock);
    return result;
}

static const struct file_operations crv_capture_file_operations = {
    .owner = THIS_MODULE,
    .read = crv_capture_read,
    .poll = crv_capture_poll,
    .open = crv_capture_open,
    .release = crv_capture_release,
};
static struct miscdevice crv_capture_misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "crv-input-capture",
    .fops = &crv_capture_file_operations,
    .mode = 0400,
};

static unsigned int crv_capture_events(struct input_handle* handle, struct input_value* values, unsigned int count)
{
    struct crv_capture_source_t* source = handle->private;
    ktime_t* timestamps;
    u64 timestamp_ns;
    u64 sequence;
    unsigned long flags;
    unsigned int index;

    if (!count) return 0;

    /*
        Use the report timestamp retained by the input core. If the lower driver did not supply one with
        input_set_timestamp(), the first call synthesizes a CLOCK_MONOTONIC timestamp here.
    */
    timestamps = input_get_timestamp(handle->dev);
    timestamp_ns = (u64)ktime_to_ns(timestamps[INPUT_CLK_MONO]);

    spin_lock_irqsave(&crv_capture.lock, flags);

    if (!crv_capture.capture_active || crv_capture.stream_failed || crv_capture.source != source)
    {
        spin_unlock_irqrestore(&crv_capture.lock, flags);
        return count;
    }

    sequence = crv_capture.next_batch_sequence++;

    /*
        The callback is committed atomically: either every input_value enters the FIFO, or no value from this callback
        does. Advancing the sequence before this check makes the loss visible in the next successful batch.
    */
    if (kfifo_avail(&crv_capture_fifo) < count)
    {
        crv_capture.dropped_batches++;
        crv_capture.dropped_values += count;
        spin_unlock_irqrestore(&crv_capture.lock, flags);
        return count;
    }

    for (index = 0; index < count; ++index)
    {
        struct crv_capture_event_t event = {
            .timestamp_ns = timestamp_ns,
            .batch_sequence = sequence,
            .type = values[index].type,
            .code = values[index].code,
            .value = values[index].value,
        };

        /*
            The availability check above cannot become false: this is the only producer, it is serialized by
            crv_capture.lock, and the reader only creates more space.
        */
        if (WARN_ON_ONCE(!kfifo_put(&crv_capture_fifo, event))) break;
    }

    spin_unlock_irqrestore(&crv_capture.lock, flags);

    wake_up_interruptible(&crv_capture.read_wait);

    return count;
}

static int crv_capture_connect(
    struct input_handler* handler, struct input_dev* device, const struct input_device_id* id)
{
    struct crv_capture_source_t* source;
    unsigned long flags;
    int error;

    (void)id;

    /*
        input core serializes connect() and disconnect(). The state lock is still needed because open() may inspect
        source concurrently.
    */
    spin_lock_irqsave(&crv_capture.lock, flags);
    if (crv_capture.source)
    {
        spin_unlock_irqrestore(&crv_capture.lock, flags);
        return -ENODEV;
    }
    spin_unlock_irqrestore(&crv_capture.lock, flags);

    source = kzalloc(sizeof(*source), GFP_KERNEL);
    if (!source) return -ENOMEM;

    source->handle.dev = device;
    source->handle.handler = handler;
    source->handle.name = "crv-input-capture";
    source->handle.private = source;

    error = input_register_handle(&source->handle);
    if (error) goto err_free_source;

    error = input_open_device(&source->handle);
    if (error) goto err_unregister_handle;

    spin_lock_irqsave(&crv_capture.lock, flags);

    /*
        A second successful connect cannot race us because input core serializes connect callbacks. Keep the check as a
        defensive invariant.
    */
    if (WARN_ON_ONCE(crv_capture.source))
    {
        spin_unlock_irqrestore(&crv_capture.lock, flags);
        error = -EBUSY;
        goto err_close_device;
    }

    crv_capture.source = source;

    spin_unlock_irqrestore(&crv_capture.lock, flags);

    pr_info("attached to input device '%s', phys '%s', bus %04x vendor %04x product %04x version %04x\n",
        device->name ? device->name : "<unnamed>", device->phys ? device->phys : "<unknown>", device->id.bustype,
        device->id.vendor, device->id.product, device->id.version);

    return 0;

err_close_device:
    input_close_device(&source->handle);
err_unregister_handle:
    input_unregister_handle(&source->handle);
err_free_source:
    kfree(source);
    return error;
}

static void crv_capture_disconnect(struct input_handle* handle)
{
    struct crv_capture_source_t* source = handle->private;
    struct input_dev* device = handle->dev;
    unsigned long flags;

    spin_lock_irqsave(&crv_capture.lock, flags);

    if (crv_capture.source == source)
    {
        crv_capture.source = NULL;

        if (crv_capture.capture_active)
        {
            crv_capture.capture_active = false;
            crv_capture.stream_failed = true;
        }
    }

    spin_unlock_irqrestore(&crv_capture.lock, flags);

    wake_up_interruptible(&crv_capture.read_wait);

    /*
        On handler removal, input_close_device() waits for in-flight delivery. On physical device removal, input core
        has already disabled this handle while holding the device event lock. In both cases source remains alive until
        no callback can still be using it.
    */
    input_close_device(handle);
    input_unregister_handle(handle);

    pr_info("detached from input device '%s', phys '%s'\n", device->name ? device->name : "<unnamed>",
        device->phys ? device->phys : "<unknown>");

    kfree(source);
}

static const struct input_device_id crv_capture_device_ids[] = {
    {
        .flags = INPUT_DEVICE_ID_MATCH_EVBIT | INPUT_DEVICE_ID_MATCH_RELBIT,
        .evbit = {[BIT_WORD(EV_REL)] = BIT_MASK(EV_REL)},
        .relbit = {[BIT_WORD(REL_X)] = BIT_MASK(REL_X) | BIT_MASK(REL_Y)},
    },
    {},
};
MODULE_DEVICE_TABLE(input, crv_capture_device_ids);

static struct input_handler crv_capture_input_handler = {
    .events = crv_capture_events,
    .connect = crv_capture_connect,
    .disconnect = crv_capture_disconnect,
    .name = "crv-input-capture",
    .id_table = crv_capture_device_ids,
};

static int __init crv_init(void)
{
    int error;

    spin_lock_init(&crv_capture.lock);
    init_waitqueue_head(&crv_capture.read_wait);
    atomic_set(&crv_capture.opened, 0);

    crv_capture.source = NULL;
    crv_capture.capture_active = false;
    crv_capture.stream_failed = false;
    crv_capture.next_batch_sequence = 0;
    crv_capture.dropped_batches = 0;
    crv_capture.dropped_values = 0;

    INIT_KFIFO(crv_capture_fifo);

    error = misc_register(&crv_capture_misc_device);
    if (error)
    {
        pr_err("failed to register capture device: %d\n", error);
        return error;
    }

    error = input_register_handler(&crv_capture_input_handler);
    if (error)
    {
        pr_err("failed to register input handler: %d\n", error);
        misc_deregister(&crv_capture_misc_device);
        return error;
    }

    pr_info("loaded; stream device is /dev/%s\n", crv_capture_misc_device.name);

    return 0;
}

static void __exit crv_exit(void)
{
    input_unregister_handler(&crv_capture_input_handler);
    misc_deregister(&crv_capture_misc_device);

    pr_info("unloaded\n");
}

module_init(crv_init);
module_exit(crv_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Frank Secilia");
MODULE_DESCRIPTION("Curves Mouse Acceleration Input Handler");
MODULE_VERSION(__stringify(CRV_VERSION));
