// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#define pr_fmt(fmt) KBUILD_MODNAME ": capture: " fmt

#include <crv/kernel/input/abi.h>
#include <crv/kernel/input/capture/abi.h>
#include <crv/kernel/input/capture/capture.h>

#include <linux/atomic.h>
#include <linux/compiler.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/kfifo.h>
#include <linux/ktime.h>
#include <linux/limits.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/printk.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/wait.h>

/// number of bytes retained in callback-frame FIFO
///
/// For a mouse producing 4 input values per callback at 4 kHz, each frame occupies 64 bytes.This 2 MiB FIFO holds ~8 s.
#define CRV_CAPTURE_FIFO_BYTE_COUNT (2u * 1024u * 1024u)

/// subsystem state
///
/// #Synchronization
///
/// Lock ordering: read_mutex -> state_lock.
///
/// state_lock protects every field below except opened, serializes the sole FIFO producer, and excludes producer
/// insertion during the open()-time FIFO reset.
///
/// read_mutex serializes FIFO consumers, including tasks sharing an open file description.
///
/// opened is a single-open ownership token and publishes no other state; session handoff synchronizes through
/// state_lock, which open() takes before touching anything else. kfifo tolerates 1 producer and 1 consumer without a
/// shared lock. kfifo_reset() runs only in open(), where the token excludes the previous session's consumer and
/// state_lock excludes the producer.
///
/// Lockless READ_ONCE observations (record() fast path, poll(), the wait predicate) are advisory. Every consequential
/// decision is remade under state_lock.
///
/// source is borrowed from the input handler. detach() clears it before input_close_device(),
/// input_unregister_handle(), and freeing the handle; lockless readers never dereference it.
struct crv_capture_state_t
{
    spinlock_t state_lock;
    struct mutex read_mutex;

    /// wakes reader when the fifo gains data or the session terminates
    wait_queue_head_t read_wait;

    /// single-open ownership token
    atomic_t opened;

    /// borrowed source identity; owned by the input handler
    struct input_handle* source;

    /// snapshot of source->dev->max_vals taken at attach
    unsigned int value_capacity;

    /// true while an open stream accepts new callback frames
    bool session_active;

    /// positive errno reported once buffered bytes have drained; 0 means none
    int stream_error;

    /// sequence assigned to next observed callback
    u64 next_sequence;

    u64 dropped_callbacks;
    u64 dropped_values;
};

static struct crv_capture_state_t crv_capture;

static DEFINE_KFIFO(crv_capture_fifo, unsigned char, CRV_CAPTURE_FIFO_BYTE_COUNT);

void crv_log_timestamp_regression(
    crv_u64_t previous_timestamp, crv_u64_t observed_timestamp, crv_u64_t repaired_timestamp)
{
    pr_warn_ratelimited("crv: input timestamp regressed from %llu ns to %llu ns; "
                        "continued logical clock at %llu ns\n",
        (unsigned long long)previous_timestamp, (unsigned long long)observed_timestamp,
        (unsigned long long)repaired_timestamp);
}

//
// stream layout: stream header -> callback frames -> buffered drain -> terminal errno, if any
//

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

/// signals stream failure under lock
///
/// Readers confirm both stores under the same lock, so the pair needs no independent ordering.
///
/// \pre callers hold crv_capture.state_lock
static void crv_capture_fail_stream_locked(int error)
{
    lockdep_assert_held(&crv_capture.state_lock);

    WRITE_ONCE(crv_capture.stream_error, error);
    WRITE_ONCE(crv_capture.session_active, false);
}

/// advisory wakeup predicate; a stale observation costs 1 read-loop iteration, which rechecks under state_lock
///
/// Failure paths clear session_active in the same critical section that sets stream_error, so session_active alone
/// covers both.
static bool crv_capture_read_ready(void)
{
    return !kfifo_is_empty(&crv_capture_fifo) || !READ_ONCE(crv_capture.session_active);
}

static int crv_capture_open(struct inode* inode, struct file* file)
{
    struct crv_capture_stream_header_t header;
    unsigned long flags;
    unsigned int inserted;
    int error;

    if (atomic_cmpxchg(&crv_capture.opened, 0, 1) != 0) return -EBUSY;

    error = stream_open(inode, file);
    if (error) goto err_release_open;

    crv_capture_initialize_stream_header(&header);

    spin_lock_irqsave(&crv_capture.state_lock, flags);

    if (!crv_capture.source)
    {
        error = -ENODEV;
        goto err_unlock;
    }

    // token excludes the previous session's consumer; this lock excludes the producer
    kfifo_reset(&crv_capture_fifo);

    crv_capture.next_sequence = 0;
    crv_capture.dropped_callbacks = 0;
    crv_capture.dropped_values = 0;
    WRITE_ONCE(crv_capture.stream_error, 0);

    inserted = kfifo_in(&crv_capture_fifo, (unsigned char const*)&header, sizeof(header));

    if (WARN_ON_ONCE(inserted != sizeof(header)))
    {
        // nothing can see this session yet; reset rather than expose a partial header
        kfifo_reset(&crv_capture_fifo);
        error = -EIO;
        goto err_unlock;
    }

    // activation last; producers insert only while session_active, so the completed header precedes every frame
    WRITE_ONCE(crv_capture.session_active, true);

    spin_unlock_irqrestore(&crv_capture.state_lock, flags);

    pr_info("stream opened\n");
    return 0;

err_unlock:
    spin_unlock_irqrestore(&crv_capture.state_lock, flags);

err_release_open:
    atomic_set(&crv_capture.opened, 0);
    return error;
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

    error = mutex_lock_interruptible(&crv_capture.read_mutex);
    if (error) return error;

    requested = min_t(size_t, count, UINT_MAX);

    for (;;)
    {
        if (!kfifo_is_empty(&crv_capture_fifo)) break;

        // Terminal state is sticky: session_active cannot return to true within this session, buffered bytes drain
        // first, and the first empty read reports the errno recorded under state_lock.
        if (!READ_ONCE(crv_capture.session_active))
        {
            spin_lock_irqsave(&crv_capture.state_lock, flags);
            error = crv_capture.stream_error;
            spin_unlock_irqrestore(&crv_capture.state_lock, flags);

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

    // copied == 0 without an error should be impossible for a non-empty FIFO; report it rather than return 0
    if (copied) result = (ssize_t)copied;
    else if (error) result = error;
    else result = -EIO;

out_unlock:
    mutex_unlock(&crv_capture.read_mutex);
    return result;
}

static __poll_t crv_capture_poll(struct file* file, poll_table* wait)
{
    __poll_t mask = 0;

    poll_wait(file, &crv_capture.read_wait, wait);

    // Advisory; may transiently report EPOLLHUP without EPOLLERR on weakly ordered hardware. read() reports the
    // authoritative errno under state_lock.
    if (!kfifo_is_empty(&crv_capture_fifo)) mask |= EPOLLIN | EPOLLRDNORM;
    if (READ_ONCE(crv_capture.stream_error)) mask |= EPOLLERR;
    if (!READ_ONCE(crv_capture.session_active)) mask |= EPOLLHUP;

    return mask;
}

static int crv_capture_release(struct inode* inode, struct file* file)
{
    unsigned long flags;
    u64 dropped_callbacks;
    u64 dropped_values;

    (void)inode;
    (void)file;

    spin_lock_irqsave(&crv_capture.state_lock, flags);

    WRITE_ONCE(crv_capture.session_active, false);

    dropped_callbacks = crv_capture.dropped_callbacks;
    dropped_values = crv_capture.dropped_values;

    spin_unlock_irqrestore(&crv_capture.state_lock, flags);

    wake_up_interruptible(&crv_capture.read_wait);

    // release token last: a new open() must not reset the FIFO before this session has quiesced
    atomic_set(&crv_capture.opened, 0);

    if (dropped_callbacks)
    {
        pr_warn("stream closed after dropping %llu callbacks (%llu values)\n", (unsigned long long)dropped_callbacks,
            (unsigned long long)dropped_values);
    }
    else
    {
        pr_info("stream closed without drops\n");
    }

    return 0;
}

static const struct file_operations crv_capture_file_operations = {
    .owner = THIS_MODULE,
    .open = crv_capture_open,
    .read = crv_capture_read,
    .poll = crv_capture_poll,
    .release = crv_capture_release,
};

static struct miscdevice crv_capture_misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = CRV_CAPTURE_NAME,
    .fops = &crv_capture_file_operations,
    .mode = 0400,
};

int crv_capture_register(void)
{
    int error;

    spin_lock_init(&crv_capture.state_lock);
    mutex_init(&crv_capture.read_mutex);
    init_waitqueue_head(&crv_capture.read_wait);

    atomic_set(&crv_capture.opened, 0);

    crv_capture.source = NULL;
    crv_capture.value_capacity = 0;
    crv_capture.session_active = false;
    crv_capture.stream_error = 0;
    crv_capture.next_sequence = 0;
    crv_capture.dropped_callbacks = 0;
    crv_capture.dropped_values = 0;

    kfifo_reset(&crv_capture_fifo);

    error = misc_register(&crv_capture_misc_device);
    if (error)
    {
        pr_err("failed to register stream device: %d\n", error);
        return error;
    }

    pr_info("registered stream device /dev/%s\n", crv_capture_misc_device.name);

    return 0;
}

int crv_capture_attach(struct input_handle* handle)
{
    struct input_dev* device;
    unsigned long flags;
    u64 maximum_frame_size;

    if (WARN_ON_ONCE(!handle || !handle->dev)) return -EINVAL;

    device = handle->dev;

    // bounce devices with no input capacity
    if (!device->max_vals)
    {
        pr_warn(
            "rejecting input device '%s': input-value capacity is zero\n", device->name ? device->name : "<unnamed>");

        return -EINVAL;
    }

    // guarantee record()'s unsigned-int frame-size arithmetic cannot overflow for count <= value_capacity
    maximum_frame_size
        = sizeof(struct crv_capture_input_values_header_t) + (u64)device->max_vals * sizeof(struct crv_input_value_t);
    if (maximum_frame_size > CRV_CAPTURE_FIFO_BYTE_COUNT)
    {
        pr_warn("rejecting input device '%s': value capacity %u requires "
                "a frame larger than the %u-byte FIFO\n",
            device->name ? device->name : "<unnamed>", device->max_vals, CRV_CAPTURE_FIFO_BYTE_COUNT);

        return -E2BIG;
    }

    spin_lock_irqsave(&crv_capture.state_lock, flags);

    // bounce if capture is already connected
    if (crv_capture.source)
    {
        spin_unlock_irqrestore(&crv_capture.state_lock, flags);
        return -EBUSY;
    }

    crv_capture.value_capacity = device->max_vals;
    WRITE_ONCE(crv_capture.source, handle);

    spin_unlock_irqrestore(&crv_capture.state_lock, flags);

    return 0;
}

void crv_capture_record(
    struct input_handle* handle, const struct input_value* values, unsigned int count, crv_u64_t timestamp_ns)
{
    struct crv_capture_input_values_header_t frame_header;
    u64 sequence;
    unsigned long flags;
    unsigned int payload_size;
    unsigned int frame_size;
    unsigned int inserted;

    if (!count) return;

    if (WARN_ON_ONCE(!handle || !values)) return;

    // fast-path rejection only; both conditions are rechecked under state_lock
    if (READ_ONCE(crv_capture.source) != handle || !READ_ONCE(crv_capture.session_active)) return;

    spin_lock_irqsave(&crv_capture.state_lock, flags);

    if (crv_capture.source != handle || !crv_capture.session_active || crv_capture.stream_error) goto out_unlock;

    // attach() sized the stream against device->max_vals. A larger callback breaks that input-core invariant and the
    // recorded capacity can no longer be trusted, so terminate loudly.
    if (WARN_ON_ONCE(count > crv_capture.value_capacity)) goto fail_locked;

    // cannot overflow: attach() bounded the complete frame by the FIFO size
    payload_size = count * (unsigned int)sizeof(struct crv_input_value_t);
    frame_size = (unsigned int)sizeof(frame_header) + payload_size;

    // sequence advances even on drops, so callback loss appears as a gap
    sequence = crv_capture.next_sequence++;

    // frames are all or nothing: either the complete frame fits before insertion begins, or no byte enters the FIFO
    if (kfifo_avail(&crv_capture_fifo) < frame_size)
    {
        crv_capture.dropped_callbacks++;
        crv_capture.dropped_values += count;
        goto out_unlock;
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
        .value_capacity = crv_capture.value_capacity,
    };

    // Short insertion is impossible: space was checked for the complete frame, this lock serializes the sole producer,
    // the consumer only frees space, and struct input_value and crv_input_value_t layouts are asserted identical by the
    // input ABI compatibility unit. Check anyway; an impossible short insertion terminates the stream instead of
    // publishing under a broken FIFO contract.
    inserted = kfifo_in(&crv_capture_fifo, (unsigned char const*)&frame_header, sizeof(frame_header));
    if (WARN_ON_ONCE(inserted != sizeof(frame_header))) goto fail_locked;

    inserted = kfifo_in(&crv_capture_fifo, (unsigned char const*)values, payload_size);
    if (WARN_ON_ONCE(inserted != payload_size)) goto fail_locked;

    spin_unlock_irqrestore(&crv_capture.state_lock, flags);
    wake_up_interruptible(&crv_capture.read_wait);
    return;

fail_locked:
    crv_capture_fail_stream_locked(EIO);
    spin_unlock_irqrestore(&crv_capture.state_lock, flags);
    wake_up_interruptible(&crv_capture.read_wait);
    return;

out_unlock:
    spin_unlock_irqrestore(&crv_capture.state_lock, flags);
}

void crv_capture_detach(struct input_handle* handle)
{
    unsigned long flags;
    bool detached = false;

    if (WARN_ON_ONCE(!handle)) return;

    spin_lock_irqsave(&crv_capture.state_lock, flags);

    if (crv_capture.source == handle)
    {
        WRITE_ONCE(crv_capture.source, NULL);
        crv_capture.value_capacity = 0;
        detached = true;

        // already-buffered bytes drain first; read() then reports ENODEV
        if (crv_capture.session_active) crv_capture_fail_stream_locked(ENODEV);
    }

    spin_unlock_irqrestore(&crv_capture.state_lock, flags);

    if (detached) wake_up_interruptible(&crv_capture.read_wait);
}

void crv_capture_unregister(void)
{
    // The input handler unregisters first, and its disconnect callbacks detach the source. An open file pins
    // THIS_MODULE through file_operations.owner.
    WARN_ON_ONCE(READ_ONCE(crv_capture.source));
    WARN_ON_ONCE(atomic_read(&crv_capture.opened));
    WARN_ON_ONCE(READ_ONCE(crv_capture.session_active));

    misc_deregister(&crv_capture_misc_device);

    pr_info("unregistered stream device\n");
}
