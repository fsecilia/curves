// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \brief kernel module entry point
/// \copyright Copyright (C) 2026 Frank Secilia

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <crv/kernel/input/abi.h>
#include <crv/kernel/input/capture/abi.h>

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
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/stringify.h>
#include <linux/types.h>
#include <linux/wait.h>

/// device, handle, and handler name; also the /dev node name
#define CRV_CAPTURE_NAME "crv-input-capture"

/// number of bytes retained in the callback-frame stream FIFO
///
/// For a mouse producing four input values per callback at 4 kHz, each frame occupies 64 bytes and this 2 MiB FIFO
/// holds approximately eight seconds.
#define CRV_CAPTURE_FIFO_BYTE_COUNT (2u * 1024u * 1024u)

struct crv_capture_source_t
{
    struct input_handle handle;
    unsigned int value_capacity;
};

struct crv_capture_state_t
{
    spinlock_t lock;
    wait_queue_head_t read_wait;
    atomic_t opened;

    struct crv_capture_source_t* source;

    bool capture_active;

    /// positive errno reported after buffered bytes have been drained; zero means no terminal stream error
    int stream_error;

    u64 next_sequence;
    u64 dropped_callbacks;
    u64 dropped_values;
};

static struct crv_capture_state_t crv_capture;

static DEFINE_KFIFO(crv_capture_fifo, unsigned char, CRV_CAPTURE_FIFO_BYTE_COUNT);

// stream is single-open, but multiple concurrent readers must still be protected against
static DEFINE_MUTEX(crv_capture_read_lock);

static void crv_capture_initialize_stream_header(struct crv_capture_stream_header_t* header)
{
    memset(header, 0, sizeof(*header));
    memcpy(header->magic, CRV_CAPTURE_STREAM_MAGIC, sizeof(header->magic));

    header->format_version = CRV_CAPTURE_FORMAT_VERSION;
    header->header_size = sizeof(*header);
    header->input_value_size = sizeof(struct crv_input_value_t);
    header->clock_id = CRV_CAPTURE_CLOCK_MONOTONIC;
    header->byte_order_marker = CRV_CAPTURE_BYTE_ORDER_MARKER;
}

/*
    Callers hold crv_capture.lock. Readers that observe capture_active cleared confirm stream_error under the same
    lock, so the two stores need no ordering of their own.
*/
static void crv_capture_fail_stream_locked(int error)
{
    WRITE_ONCE(crv_capture.stream_error, error);
    WRITE_ONCE(crv_capture.capture_active, false);
}

/*
    Wakeup condition only. The unlocked reads may observe the flags inconsistently on weakly ordered hardware; a
    spurious wake costs one loop iteration in read(), which re-evaluates under proper ordering.
*/
static bool crv_capture_read_ready(void)
{
    return !kfifo_is_empty(&crv_capture_fifo) || READ_ONCE(crv_capture.stream_error)
        || !READ_ONCE(crv_capture.capture_active);
}

static int crv_capture_open(struct inode* inode, struct file* file)
{
    struct crv_capture_stream_header_t header;
    unsigned long flags;
    int error;

    if (atomic_cmpxchg(&crv_capture.opened, 0, 1) != 0) return -EBUSY;

    error = stream_open(inode, file);
    if (error) goto err_release_open;

    crv_capture_initialize_stream_header(&header);

    spin_lock_irqsave(&crv_capture.lock, flags);

    if (!crv_capture.source)
    {
        error = -ENODEV;
        goto err_unlock;
    }

    // reset occurs under same lock producer uses, and single-open token guarantees no readers from a previous session
    kfifo_reset(&crv_capture_fifo);

    crv_capture.next_sequence = 0;
    crv_capture.dropped_callbacks = 0;
    crv_capture.dropped_values = 0;
    WRITE_ONCE(crv_capture.stream_error, 0);

    /*
        The header insertion must stay inside this critical section, before capture_active is set. The producer
        inserts only under this lock and only while capture_active is set, so no frame can precede the header in
        FIFO order. The insertion cannot be short: the FIFO was just reset and is far larger than the header.
    */
    kfifo_in(&crv_capture_fifo, (unsigned char const*)&header, sizeof(header));

    /*
        Enabling capture is the final operation. All stream state and the header are in place before the producer
        can publish the first callback frame.
    */
    WRITE_ONCE(crv_capture.capture_active, true);

    spin_unlock_irqrestore(&crv_capture.lock, flags);

    pr_info("capture stream opened\n");
    return 0;

err_unlock:
    spin_unlock_irqrestore(&crv_capture.lock, flags);
err_release_open:
    atomic_set(&crv_capture.opened, 0);
    return error;
}

static int crv_capture_release(struct inode* inode, struct file* file)
{
    unsigned long flags;
    u64 dropped_callbacks;
    u64 dropped_values;

    (void)inode;
    (void)file;

    spin_lock_irqsave(&crv_capture.lock, flags);

    WRITE_ONCE(crv_capture.capture_active, false);
    dropped_callbacks = crv_capture.dropped_callbacks;
    dropped_values = crv_capture.dropped_values;

    spin_unlock_irqrestore(&crv_capture.lock, flags);

    wake_up_interruptible(&crv_capture.read_wait);

    /*
        Releasing the open token is last: a new open() must not begin resetting the FIFO until this session is
        quiesced.
    */
    atomic_set(&crv_capture.opened, 0);

    if (dropped_callbacks)
    {
        pr_warn("capture stream closed after dropping %llu callbacks (%llu values)\n",
            (unsigned long long)dropped_callbacks, (unsigned long long)dropped_values);
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

    /*
        Unlocked reads: on weakly ordered hardware, poll may transiently report EPOLLHUP without EPOLLERR after a
        stream failure. Poll is advisory; read() reports the authoritative errno under the state lock.
    */
    if (!kfifo_is_empty(&crv_capture_fifo)) mask |= EPOLLIN | EPOLLRDNORM;
    if (READ_ONCE(crv_capture.stream_error)) mask |= EPOLLERR;
    if (!READ_ONCE(crv_capture.capture_active)) mask |= EPOLLHUP;

    return mask;
}

static ssize_t crv_capture_read(struct file* file, char __user* buffer, size_t count, loff_t* position)
{
    unsigned int copied = 0;
    unsigned int requested;
    unsigned long flags;
    ssize_t result;
    int error;

    (void)position;

    if (count == 0) return 0;

    error = mutex_lock_interruptible(&crv_capture_read_lock);
    if (error) return error;

    requested = count > UINT_MAX ? UINT_MAX : (unsigned int)count;

    for (;;)
    {
        if (!kfifo_is_empty(&crv_capture_fifo)) break;

        /*
            The unlocked read of capture_active is only a hint; it cannot return to true within a session because
            only open() sets it and the single-open token excludes a concurrent open. Taking the state lock pairs
            with the producer's unlock, making the stream_error stored before capture_active was cleared visible.

            Terminal state is sticky for the session: buffered bytes drain first, and the first empty read reports
            the error. Every clear of capture_active reachable here stores a nonzero error first -- release() cannot
            race an active read because the file reference held by the system call delays it -- so the -EIO default
            is unreachable and kept only as the terminal fallback.
        */
        if (!READ_ONCE(crv_capture.capture_active))
        {
            spin_lock_irqsave(&crv_capture.lock, flags);
            error = crv_capture.stream_error;
            spin_unlock_irqrestore(&crv_capture.lock, flags);

            result = error ? -error : -EIO;
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
    .name = CRV_CAPTURE_NAME,
    .fops = &crv_capture_file_operations,
    .mode = 0400,
};

static unsigned int crv_capture_events(struct input_handle* handle, struct input_value* values, unsigned int count)
{
    struct crv_capture_source_t* source = handle->private;
    struct crv_capture_input_values_header_t frame_header;
    ktime_t* timestamps;
    u64 timestamp_ns;
    u64 sequence;
    unsigned long flags;
    unsigned int frame_size;

    if (!count) return 0;

    /*
        Avoid timestamp work when no capture session is active. The state is checked again under the producer lock.
    */
    if (!READ_ONCE(crv_capture.capture_active)) return count;

    /*
        Use the report timestamp retained by the input core. If the lower driver did not supply one with
        input_set_timestamp(), the first call synthesizes a CLOCK_MONOTONIC timestamp here.
    */
    timestamps = input_get_timestamp(handle->dev);
    timestamp_ns = (u64)ktime_to_ns(timestamps[INPUT_CLK_MONO]);

    spin_lock_irqsave(&crv_capture.lock, flags);

    if (!crv_capture.capture_active || crv_capture.stream_error || crv_capture.source != source)
    {
        spin_unlock_irqrestore(&crv_capture.lock, flags);
        return count;
    }

    /*
        Input-core invariant rather than an ordinary stream-loss condition: connect() sized everything against
        max_vals. A violation means the recorded capacity cannot be trusted; stop the stream loudly rather than
        produce data under a broken contract.
    */
    if (WARN_ON_ONCE(count > source->value_capacity))
    {
        crv_capture_fail_stream_locked(EIO);
        spin_unlock_irqrestore(&crv_capture.lock, flags);
        wake_up_interruptible(&crv_capture.read_wait);
        return count;
    }

    /*
        Cannot overflow: connect() bounded the maximum frame for max_vals values by the FIFO size, and count is at
        most max_vals.
    */
    frame_size = (unsigned int)sizeof(frame_header) + count * (unsigned int)sizeof(struct crv_input_value_t);

    /*
        The sequence advances even when the frame is dropped, making the loss visible as a gap in the next
        successful frame.
    */
    sequence = crv_capture.next_sequence++;

    /*
        The callback is committed atomically with respect to FIFO capacity: either enough space exists for the
        complete frame before any insertion begins, or no byte from this callback enters the FIFO.
    */
    if (kfifo_avail(&crv_capture_fifo) < frame_size)
    {
        crv_capture.dropped_callbacks++;
        crv_capture.dropped_values += count;
        spin_unlock_irqrestore(&crv_capture.lock, flags);
        return count;
    }

    frame_header = (struct crv_capture_input_values_header_t){
        .frame = {
            .frame_size = frame_size,
            .frame_type = CRV_CAPTURE_FRAME_TYPE_INPUT_VALUES,
            .header_size = sizeof(frame_header),
        },
        .timestamp_ns = timestamp_ns,
        .sequence = sequence,
        .value_count = count,
        .value_capacity = source->value_capacity,
    };

    /*
        Neither insertion can be short: the availability check covered the complete frame, this callback is the sole
        producer, it holds the producer lock, and the reader can only create more space. struct input_value and
        crv_input_value_t have identical layouts, asserted by the input ABI compatibility unit.
    */
    kfifo_in(&crv_capture_fifo, (unsigned char const*)&frame_header, sizeof(frame_header));
    kfifo_in(&crv_capture_fifo, (unsigned char const*)values, frame_size - (unsigned int)sizeof(frame_header));

    spin_unlock_irqrestore(&crv_capture.lock, flags);

    wake_up_interruptible(&crv_capture.read_wait);

    return count;
}

static int crv_capture_connect(
    struct input_handler* handler, struct input_dev* device, const struct input_device_id* id)
{
    struct crv_capture_source_t* source;
    unsigned long flags;
    u64 maximum_frame_size;
    int error;

    (void)id;

    if (!device->max_vals)
    {
        pr_warn(
            "rejecting input device '%s': input-value capacity is zero\n", device->name ? device->name : "<unnamed>");
        return -EINVAL;
    }

    /*
        The FIFO bound is far below UINT_MAX, so this single comparison also guarantees the per-callback frame-size
        arithmetic in crv_capture_events() cannot overflow.
    */
    maximum_frame_size
        = sizeof(struct crv_capture_input_values_header_t) + (u64)device->max_vals * sizeof(struct crv_input_value_t);

    if (maximum_frame_size > CRV_CAPTURE_FIFO_BYTE_COUNT)
    {
        pr_warn("rejecting input device '%s': value capacity %u requires a frame larger than the %u-byte FIFO\n",
            device->name ? device->name : "<unnamed>", device->max_vals, CRV_CAPTURE_FIFO_BYTE_COUNT);
        return -E2BIG;
    }

    /*
        Input core serializes connect() and disconnect(). The state lock is still needed because open() may inspect
        source concurrently.

        In this build, the first matching device wins for the duration of its attachment. When the selection UI comes
        online, user mode will choose among eligible candidates.
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
    source->handle.name = CRV_CAPTURE_NAME;
    source->handle.private = source;
    source->value_capacity = device->max_vals;

    error = input_register_handle(&source->handle);
    if (error) goto err_free_source;

    error = input_open_device(&source->handle);
    if (error) goto err_unregister_handle;

    /*
        No second connect can have raced the check above: input core serializes connect callbacks. The lock only
        orders the assignment against open().
    */
    spin_lock_irqsave(&crv_capture.lock, flags);
    crv_capture.source = source;
    spin_unlock_irqrestore(&crv_capture.lock, flags);

    pr_info("attached to input device '%s', phys '%s', bus %04x vendor %04x product %04x version %04x, "
            "input-value capacity %u\n",
        device->name ? device->name : "<unnamed>", device->phys ? device->phys : "<unknown>", device->id.bustype,
        device->id.vendor, device->id.product, device->id.version, source->value_capacity);

    return 0;

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

        if (crv_capture.capture_active) crv_capture_fail_stream_locked(ENODEV);
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

static bool crv_capture_match(struct input_handler* handler, struct input_dev* device)
{
    (void)handler;

    /*
        Accept conventional external mice and virtual relative-pointer streams produced by input remappers.
        USB includes wired mice and 2.4 GHz receiver dongles.
    */
    switch (device->id.bustype)
    {
        case BUS_USB:
        case BUS_BLUETOOTH:
        case BUS_VIRTUAL: break;

        default: return false;
    }

    // reject device classes that may expose mouse-like compatibility capabilities but are not physical mice
    if (test_bit(INPUT_PROP_DIRECT, device->propbit) || test_bit(INPUT_PROP_BUTTONPAD, device->propbit)
        || test_bit(INPUT_PROP_SEMI_MT, device->propbit) || test_bit(INPUT_PROP_TOPBUTTONPAD, device->propbit)
        || test_bit(INPUT_PROP_POINTING_STICK, device->propbit) || test_bit(INPUT_PROP_ACCELEROMETER, device->propbit))
    {
        return false;
    }

#ifdef INPUT_PROP_PRESSUREPAD
    if (test_bit(INPUT_PROP_PRESSUREPAD, device->propbit)) return false;
#endif

    // catch touch and tablet nodes whose properties are absent or incomplete
    if (test_bit(BTN_TOUCH, device->keybit) || test_bit(BTN_TOOL_FINGER, device->keybit)
        || test_bit(BTN_TOOL_PEN, device->keybit) || test_bit(BTN_TOOL_MOUSE, device->keybit))
    {
        return false;
    }

    return true;
}

static const struct input_device_id crv_capture_device_ids[] = {
    {
        .flags = INPUT_DEVICE_ID_MATCH_EVBIT | INPUT_DEVICE_ID_MATCH_KEYBIT | INPUT_DEVICE_ID_MATCH_RELBIT,
        .evbit = {[BIT_WORD(EV_KEY)] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REL)},
        .keybit = {[BIT_WORD(BTN_LEFT)] = BIT_MASK(BTN_LEFT)},
        .relbit = {[BIT_WORD(REL_X)] = BIT_MASK(REL_X) | BIT_MASK(REL_Y)},
    },
    {},
};
MODULE_DEVICE_TABLE(input, crv_capture_device_ids);

static struct input_handler crv_capture_input_handler = {
    .events = crv_capture_events,
    .connect = crv_capture_connect,
    .disconnect = crv_capture_disconnect,
    .match = crv_capture_match,
    .name = CRV_CAPTURE_NAME,
    .id_table = crv_capture_device_ids,
};

static int __init crv_init(void)
{
    int error;

    // the remaining state fields and the fifo are statically zero-initialized
    spin_lock_init(&crv_capture.lock);
    init_waitqueue_head(&crv_capture.read_wait);

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

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Frank Secilia");
MODULE_DESCRIPTION("Curves Mouse Acceleration Input Handler");
MODULE_VERSION(__stringify(CRV_VERSION));
