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
#include <linux/uaccess.h>
#include <linux/wait.h>

/// device, handle, and handler name; also the /dev node name
#define CRV_CAPTURE_NAME "crv-input-capture"

/// number of bytes retained in the callback-frame stream FIFO
///
/// For a mouse producing four input values per callback at 4 kHz, each frame occupies 64 bytes and this 2 MiB FIFO
/// holds approximately eight seconds.
#define CRV_CAPTURE_FIFO_BYTE_COUNT (2u * 1024u * 1024u)

static bool crv_capture_allow_i8042;
module_param_named(allow_i8042, crv_capture_allow_i8042, bool, 0444);
MODULE_PARM_DESC(allow_i8042, "Allow legacy i8042 relative-pointer devices; intended for virtual-machine testing.");

struct crv_capture_source_t
{
    struct input_handle handle;
    unsigned int value_capacity;
};

struct crv_capture_reader_t
{
    struct crv_capture_stream_header_t header;
    size_t header_offset;
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

static DECLARE_KFIFO(crv_capture_fifo, unsigned char, CRV_CAPTURE_FIFO_BYTE_COUNT);

static bool crv_capture_calculate_input_values_frame_size(unsigned int value_count, unsigned int* frame_size)
{
    u64 const calculated_size
        = sizeof(struct crv_capture_input_values_header_t) + (u64)value_count * sizeof(struct crv_input_value_t);

    if (calculated_size > UINT_MAX) return false;

    *frame_size = (unsigned int)calculated_size;
    return true;
}

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

static void crv_capture_fail_stream_locked(int error)
{
    WRITE_ONCE(crv_capture.stream_error, error);
    WRITE_ONCE(crv_capture.capture_active, false);
}

static bool crv_capture_read_ready(void)
{
    return !kfifo_is_empty(&crv_capture_fifo) || READ_ONCE(crv_capture.stream_error)
        || !READ_ONCE(crv_capture.capture_active);
}

static int crv_capture_open(struct inode* inode, struct file* file)
{
    struct crv_capture_reader_t* reader;
    unsigned long flags;
    int error;

    if (atomic_cmpxchg(&crv_capture.opened, 0, 1) != 0) return -EBUSY;

    reader = kzalloc(sizeof(*reader), GFP_KERNEL);
    if (!reader)
    {
        error = -ENOMEM;
        goto err_release_open;
    }

    error = stream_open(inode, file);
    if (error) goto err_free_reader;

    crv_capture_initialize_stream_header(&reader->header);

    spin_lock_irqsave(&crv_capture.lock, flags);

    if (!crv_capture.source)
    {
        error = -ENODEV;
        goto err_unlock;
    }

    /*
        This occurs under the same lock the producer uses, and the single-open token guarantees that no reader from a
        previous session can still be consuming the FIFO.
    */
    kfifo_reset(&crv_capture_fifo);

    crv_capture.next_sequence = 0;
    crv_capture.dropped_callbacks = 0;
    crv_capture.dropped_values = 0;
    WRITE_ONCE(crv_capture.stream_error, 0);

    file->private_data = reader;

    /*
        Enabling capture is the final operation. All stream state and the per-open header are initialized before the
        producer can publish the first callback frame.
    */
    WRITE_ONCE(crv_capture.capture_active, true);

    spin_unlock_irqrestore(&crv_capture.lock, flags);

    pr_info("capture stream opened\n");
    return 0;

err_unlock:
    spin_unlock_irqrestore(&crv_capture.lock, flags);
err_free_reader:
    kfree(reader);
err_release_open:
    atomic_set(&crv_capture.opened, 0);
    return error;
}

static int crv_capture_release(struct inode* inode, struct file* file)
{
    struct crv_capture_reader_t* reader = file->private_data;
    unsigned long flags;
    u64 dropped_callbacks;
    u64 dropped_values;

    (void)inode;

    spin_lock_irqsave(&crv_capture.lock, flags);

    WRITE_ONCE(crv_capture.capture_active, false);
    dropped_callbacks = crv_capture.dropped_callbacks;
    dropped_values = crv_capture.dropped_values;

    spin_unlock_irqrestore(&crv_capture.lock, flags);

    wake_up_interruptible(&crv_capture.read_wait);

    file->private_data = NULL;
    kfree(reader);

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
    struct crv_capture_reader_t* reader = file->private_data;
    __poll_t mask = 0;

    poll_wait(file, &crv_capture.read_wait, wait);

    if (reader && READ_ONCE(reader->header_offset) < sizeof(reader->header)) { mask |= EPOLLIN | EPOLLRDNORM; }

    if (!kfifo_is_empty(&crv_capture_fifo)) mask |= EPOLLIN | EPOLLRDNORM;
    if (READ_ONCE(crv_capture.stream_error)) mask |= EPOLLERR;
    if (!READ_ONCE(crv_capture.capture_active)) mask |= EPOLLHUP;

    return mask;
}

static DEFINE_MUTEX(crv_capture_read_lock);

static ssize_t crv_capture_read_stream_header(struct crv_capture_reader_t* reader, char __user* buffer, size_t count)
{
    size_t const remaining = sizeof(reader->header) - reader->header_offset;
    size_t const requested = count < remaining ? count : remaining;
    unsigned long const not_copied
        = copy_to_user(buffer, (unsigned char const*)&reader->header + reader->header_offset, requested);
    size_t const copied = requested - not_copied;

    if (copied)
    {
        WRITE_ONCE(reader->header_offset, reader->header_offset + copied);
        return (ssize_t)copied;
    }

    return -EFAULT;
}

static ssize_t crv_capture_read(struct file* file, char __user* buffer, size_t count, loff_t* position)
{
    struct crv_capture_reader_t* reader = file->private_data;
    unsigned int copied = 0;
    unsigned int requested;
    ssize_t result;
    int error;

    (void)position;

    if (count == 0) return 0;
    if (!reader) return -EIO;

    error = mutex_lock_interruptible(&crv_capture_read_lock);
    if (error) return error;

    /*
        Return the stream header by itself. The next read begins with FIFO data. Byte sources and decoders must already
        tolerate arbitrary read boundaries, and keeping the two sources separate makes the read path much simpler.
    */
    if (reader->header_offset < sizeof(reader->header))
    {
        result = crv_capture_read_stream_header(reader, buffer, count);
        goto out_unlock;
    }

    requested = count > UINT_MAX ? UINT_MAX : (unsigned int)count;

    for (;;)
    {
        if (!kfifo_is_empty(&crv_capture_fifo)) break;

        /*
            Terminal stream state is sticky for the current open session. Buffered bytes are drained first; the first
            empty read reports the terminal error. A normal close cannot race an active read because the file reference
            held by the system call delays release().
        */
        if (!READ_ONCE(crv_capture.capture_active))
        {
            error = READ_ONCE(crv_capture.stream_error);
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
    unsigned int value_bytes;
    unsigned int inserted;

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
        These are input-core or module invariants rather than ordinary stream-loss conditions. Stop the stream loudly
        instead of silently producing data whose framing or recorded capacity cannot be trusted.
    */
    if (WARN_ON_ONCE(count > source->value_capacity)
        || WARN_ON_ONCE(!crv_capture_calculate_input_values_frame_size(count, &frame_size))
        || WARN_ON_ONCE(frame_size > kfifo_size(&crv_capture_fifo)))
    {
        crv_capture_fail_stream_locked(EIO);
        spin_unlock_irqrestore(&crv_capture.lock, flags);
        wake_up_interruptible(&crv_capture.read_wait);
        return count;
    }

    value_bytes = frame_size - sizeof(frame_header);
    sequence = crv_capture.next_sequence++;

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
        The callback is committed atomically with respect to FIFO capacity: either enough space exists for the complete
        frame before either insertion begins, or no byte from this callback enters the FIFO. Advancing the sequence
        before this check makes the loss visible in the next successful frame.
    */
    if (kfifo_avail(&crv_capture_fifo) < frame_size)
    {
        crv_capture.dropped_callbacks++;
        crv_capture.dropped_values += count;
        spin_unlock_irqrestore(&crv_capture.lock, flags);
        return count;
    }

    inserted = kfifo_in(&crv_capture_fifo, (unsigned char const*)&frame_header, sizeof(frame_header));

    if (WARN_ON_ONCE(inserted != sizeof(frame_header)))
    {
        /*
            This should be impossible after the complete-frame availability check. If it occurs, terminate the stream.
            Any bytes already inserted form a truncated final frame rather than allowing later frames to follow it.
        */
        crv_capture_fail_stream_locked(EIO);
        spin_unlock_irqrestore(&crv_capture.lock, flags);
        wake_up_interruptible(&crv_capture.read_wait);
        return count;
    }

    /*
        struct input_value and crv_input_value_t have identical layouts, asserted by the input ABI compatibility unit.
        The availability check above cannot become false: this callback is the only producer, it holds the producer
        lock, and the reader can only create more space.
    */
    inserted = kfifo_in(&crv_capture_fifo, (unsigned char const*)values, value_bytes);

    if (WARN_ON_ONCE(inserted != value_bytes))
    {
        /*
            As above, terminate rather than permit a partial frame to be followed by additional frames. The decoder can
            discard this final truncated frame.
        */
        crv_capture_fail_stream_locked(EIO);
        spin_unlock_irqrestore(&crv_capture.lock, flags);
        wake_up_interruptible(&crv_capture.read_wait);
        return count;
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
    unsigned int maximum_frame_size;
    int error;

    (void)id;

    if (!device->max_vals)
    {
        pr_warn(
            "rejecting input device '%s': input-value capacity is zero\n", device->name ? device->name : "<unnamed>");
        return -EINVAL;
    }

    if (!crv_capture_calculate_input_values_frame_size(device->max_vals, &maximum_frame_size)
        || maximum_frame_size > CRV_CAPTURE_FIFO_BYTE_COUNT)
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

    pr_info("attached to input device '%s', phys '%s', bus %04x vendor %04x product %04x version %04x, "
            "input-value capacity %u\n",
        device->name ? device->name : "<unnamed>", device->phys ? device->phys : "<unknown>", device->id.bustype,
        device->id.vendor, device->id.product, device->id.version, source->value_capacity);

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

        case BUS_I8042:
            if (!crv_capture_allow_i8042) return false;
            break;

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

    if (crv_capture_allow_i8042) { pr_warn("legacy i8042 pointer matching enabled\n"); }

    spin_lock_init(&crv_capture.lock);
    init_waitqueue_head(&crv_capture.read_wait);
    atomic_set(&crv_capture.opened, 0);

    crv_capture.source = NULL;
    crv_capture.capture_active = false;
    crv_capture.stream_error = 0;
    crv_capture.next_sequence = 0;
    crv_capture.dropped_callbacks = 0;
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

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Frank Secilia");
MODULE_DESCRIPTION("Curves Mouse Acceleration Input Handler");
MODULE_VERSION(__stringify(CRV_VERSION));
