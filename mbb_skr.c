/* mbb_skr.c - Kernel module for obtaining information about special key press events on maibenben laptops 
 * Copyright (C) 2025  Alexander Svobodov
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 * 
 * SPDX-License-Identifier: GPL-2.0
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/acpi.h>
#include <linux/slab.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/jiffies.h>

#define EVBU_SIZE        32      // 0x20 bytes as per ACPI specification
#define POLL_DELAY_MS    500     // Fixed 0.5s polling interval
#define EV20_METHOD      "\\_SB_.PCI0.WMID.EV20"

static char evbu_data[EVBU_SIZE];
static spinlock_t data_lock;
static struct kobject *evbu_kobj;
static struct delayed_work poll_work;

// Функция для вызова ACPI-метода EV20
static int call_ev20(void) {
    acpi_status status;
    struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };
    union acpi_object *obj = NULL;
    unsigned long flags;
    int ret = 0;

    status = acpi_evaluate_object(NULL, EV20_METHOD, NULL, &buffer);
    if (ACPI_FAILURE(status)) {
        pr_err_ratelimited("EV20 method execution failed: 0x%x\n", status);
        ret = -EIO;
        goto exit;
    }

    obj = buffer.pointer;
    if (!obj) {
        pr_err_ratelimited("Empty response from EV20\n");
        ret = -ENODATA;
        goto exit;
    }

    if (obj->type != ACPI_TYPE_BUFFER) {
        pr_err_ratelimited("Invalid EV20 response type: %d\n", obj->type);
        ret = -EPROTO;
        goto exit;
    }

    if (obj->buffer.length != EVBU_SIZE) {
        pr_err_ratelimited("Invalid buffer size: %d (expected %d)\n",
                         obj->buffer.length, EVBU_SIZE);
        ret = -EMSGSIZE;
        goto exit;
    }

    spin_lock_irqsave(&data_lock, flags);
    memcpy(evbu_data, obj->buffer.pointer, EVBU_SIZE);
    spin_unlock_irqrestore(&data_lock, flags);

exit:
    /* Secure cleanup: wipe temporary buffer */
    if (obj && obj->buffer.pointer)
        memzero_explicit(obj->buffer.pointer, obj->buffer.length);
    
    kfree(buffer.pointer);
    return ret;
}

static void poll_evbu(struct work_struct *work) {
    if (call_ev20()) {
        pr_warn("Error polling EVBU data\n");
        /* Maintain secure state on errors */
        unsigned long flags;
        spin_lock_irqsave(&data_lock, flags);
        memzero_explicit(evbu_data, EVBU_SIZE);
        spin_unlock_irqrestore(&data_lock, flags);
    }
    schedule_delayed_work(&poll_work, msecs_to_jiffies(POLL_DELAY_MS));
}

// Sysfs: Показать данные
static ssize_t data_show(struct kobject *kobj, 
                        struct kobj_attribute *attr, 
                        char *buf) {
    unsigned long flags;
    spin_lock_irqsave(&data_lock, flags);
    memcpy(buf, evbu_data, EVBU_SIZE);
    spin_unlock_irqrestore(&data_lock, flags);
    return EVBU_SIZE;
}

static struct kobj_attribute data_attribute = 
    __ATTR_RO(data);  // Read-only для всех

static struct attribute *attrs[] = {
    &data_attribute.attr,
    NULL,
};

static struct attribute_group attr_group = {
    .attrs = attrs,
};

// Инициализация модуля
static int __init evbu_module_init(void) {
    int ret;
    
    spin_lock_init(&data_lock);
    memset(evbu_data, 0, EVBU_SIZE);

    // Создаем kobject в /sys/kernel/mbb_skr
    evbu_kobj = kobject_create_and_add("mbb_skr", kernel_kobj);
    if (!evbu_kobj)
        return -ENOMEM;

    // Создаем sysfs атрибуты
    ret = sysfs_create_group(evbu_kobj, &attr_group);
    if (ret) {
        kobject_put(evbu_kobj);
        return ret;
    }

    // Инициализируем workqueue
    INIT_DELAYED_WORK(&poll_work, poll_evbu);
    schedule_delayed_work(&poll_work, msecs_to_jiffies(POLL_DELAY_MS));
    return 0;
}

// Выгрузка модуля
static void __exit evbu_module_exit(void) {
    cancel_delayed_work_sync(&poll_work);
    sysfs_remove_group(evbu_kobj, &attr_group);
    kobject_put(evbu_kobj);
    pr_info("Maibenben special keys reader module unloaded\n");
    /* Secure wipe before unload */
    memzero_explicit(evbu_data, EVBU_SIZE);
}

module_init(evbu_module_init);
module_exit(evbu_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alexander Svobodov");
MODULE_DESCRIPTION("Maibenben Special Keys Reader");
