// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "control.h"
#include "configuration.h"
#include "ioctl.h"
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>

static struct crv_control_attachment* crv_control_find_attachment(struct crv_control* control, u64 id)
{
    struct crv_control_attachment* attachment;

    list_for_each_entry(attachment, &control->attachments, node)
    {
        if (attachment->id == id) return attachment;
    }

    return NULL;
}

static struct crv_control_attachment* crv_control_find_attachment_after(struct crv_control* control, u64 id)
{
    struct crv_control_attachment* attachment;

    list_for_each_entry(attachment, &control->attachments, node)
    {
        if (attachment->id > id) return attachment;
    }

    return NULL;
}

static int crv_control_get_device(struct crv_control* control, void __user* argument)
{
    struct crv_control_device_v1_t result;
    struct crv_control_attachment* attachment;
    struct input_dev* device;
    int error;

    crv_u64_t after_attachment_id;

    if (copy_from_user(&result, argument, sizeof(result))) return -EFAULT;

    after_attachment_id = result.after_attachment_id;
    memset(&result, 0, sizeof(result));
    result.after_attachment_id = after_attachment_id;

    mutex_lock(&control->mutex);

    attachment = crv_control_find_attachment_after(control, after_attachment_id);
    while (attachment)
    {
        device = attachment->input->dev;
        mutex_lock(&device->mutex);

        if (!device->going_away && attachment->input->open)
        {
            result.attachment_id = attachment->id;
            result.bustype = device->id.bustype;
            result.vendor = device->id.vendor;
            result.product = device->id.product;
            result.version = device->id.version;
            error = strscpy(result.sysname, dev_name(&device->dev), sizeof(result.sysname));

            mutex_unlock(&device->mutex);
            mutex_unlock(&control->mutex);

            if (error < 0)
            {
                pr_err("input device sysname exceeds control ABI capacity\n");
                return -EOVERFLOW;
            }
            if (copy_to_user(argument, &result, sizeof(result))) return -EFAULT;
            return 0;
        }

        mutex_unlock(&device->mutex);
        attachment = crv_control_find_attachment_after(control, attachment->id);
    }

    mutex_unlock(&control->mutex);
    return -ENOENT;
}

static int crv_control_copy_configuration(
    struct crv_control_prepared_configuration* configuration, struct crv_control_apply_v1_t const* request)
{
    void __user* user_configuration = u64_to_user_ptr((u64)request->configuration);
    char __user* user_bytes = user_configuration;

    if (copy_from_user(crv_control_prepared_configuration_config(configuration), user_bytes,
            crv_control_prepared_configuration_config_size()))
    {
        return -EFAULT;
    }

    user_bytes += offsetof(struct crv_control_configuration_v1_t, gain);
    if (copy_from_user(crv_control_prepared_configuration_gain(configuration), user_bytes,
            crv_control_prepared_configuration_gain_size()))
    {
        return -EFAULT;
    }

    return 0;
}

static int crv_control_publish_configuration(struct crv_control* control, u64 attachment_id,
    struct crv_control_prepared_configuration const* configuration)
{
    struct crv_control_attachment* attachment;
    struct input_dev* device;
    unsigned long flags;
    int error = 0;

    mutex_lock(&control->mutex);

    attachment = crv_control_find_attachment(control, attachment_id);
    if (!attachment)
    {
        error = -ENODEV;
        goto out_control;
    }

    device = attachment->input->dev;
    mutex_lock(&device->mutex);

    if (device->going_away || !attachment->input->open)
    {
        error = -ENODEV;
        goto out_device;
    }

    spin_lock_irqsave(&device->event_lock, flags);
    crv_control_prepared_configuration_commit(configuration, attachment->pipeline);
    spin_unlock_irqrestore(&device->event_lock, flags);

out_device:
    mutex_unlock(&device->mutex);
out_control:
    mutex_unlock(&control->mutex);
    return error;
}

static int crv_control_apply(struct crv_control* control, void __user* argument)
{
    struct crv_control_apply_v1_t request;
    struct crv_control_prepared_configuration* configuration = NULL;
    struct crv_control_validation_result_t validation;
    void* storage = NULL;
    int error;

    if (copy_from_user(&request, argument, sizeof(request))) return -EFAULT;
    if (request.reserved) return -EINVAL;
    if (request.configuration_size != sizeof(struct crv_control_configuration_v1_t)) return -EINVAL;
    if (request.mode != CRV_CONTROL_APPLY_MODE_BYPASSED && request.mode != CRV_CONTROL_APPLY_MODE_ACTIVE)
        return -EINVAL;

    storage = kvzalloc(crv_control_prepared_configuration_storage_size(), GFP_KERNEL);
    if (!storage) return -ENOMEM;

    configuration = crv_control_prepared_configuration_construct(storage, request.mode);
    if (WARN_ON_ONCE(!configuration))
    {
        error = -EINVAL;
        goto out_free;
    }

    error = crv_control_copy_configuration(configuration, &request);
    if (error) goto out_destroy;

    validation = crv_control_prepared_configuration_validate(configuration);
    if (validation.error)
    {
        pr_err("configuration rejected: %s (error %u, segment %lld)\n",
            crv_control_validation_error_name(validation.error), validation.error, (long long)validation.segment_index);
        error = -EINVAL;
        goto out_destroy;
    }

    error = crv_control_publish_configuration(control, request.attachment_id, configuration);

out_destroy:
    crv_control_prepared_configuration_destroy(configuration);
out_free:
    kvfree(storage);
    return error;
}

static long crv_control_ioctl(struct file* file, unsigned int command, unsigned long argument)
{
    struct miscdevice* misc = file->private_data;
    struct crv_control* control = container_of(misc, struct crv_control, misc);
    void __user* user_argument = (void __user*)argument;

    switch (command)
    {
        case CRV_CONTROL_IOCTL_GET_DEVICE_V1: return crv_control_get_device(control, user_argument);
        case CRV_CONTROL_IOCTL_APPLY_V1: return crv_control_apply(control, user_argument);
        default: return -ENOTTY;
    }
}

static const struct file_operations crv_control_file_operations = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = crv_control_ioctl,
    .compat_ioctl = compat_ptr_ioctl,
};

int crv_control_register(struct crv_control* control)
{
    int error;

    mutex_init(&control->mutex);
    INIT_LIST_HEAD(&control->attachments);
    control->next_attachment_id = 1;
    control->misc = (struct miscdevice){
        .minor = MISC_DYNAMIC_MINOR,
        .name = CRV_CONTROL_DEVICE_NAME,
        .fops = &crv_control_file_operations,
    };

    error = misc_register(&control->misc);
    if (error) pr_err("failed to register control device: %d\n", error);
    return error;
}

void crv_control_unregister(struct crv_control* control)
{
    misc_deregister(&control->misc);
    WARN_ON(!list_empty(&control->attachments));
}

int crv_control_attach(struct crv_control* control, struct crv_control_attachment* attachment,
    struct input_handle* input, struct crv_pipeline* pipeline)
{
    int error = 0;

    mutex_lock(&control->mutex);

    if (!control->next_attachment_id)
    {
        error = -EOVERFLOW;
        goto out;
    }

    INIT_LIST_HEAD(&attachment->node);
    attachment->id = control->next_attachment_id++;
    attachment->input = input;
    attachment->pipeline = pipeline;
    list_add_tail(&attachment->node, &control->attachments);

out:
    mutex_unlock(&control->mutex);
    return error;
}

void crv_control_detach(struct crv_control* control, struct crv_control_attachment* attachment)
{
    mutex_lock(&control->mutex);

    list_del_init(&attachment->node);
    attachment->id = 0;
    attachment->input = NULL;
    attachment->pipeline = NULL;

    mutex_unlock(&control->mutex);
}
