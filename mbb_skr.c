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
#include <linux/delay.h>
#include <linux/workqueue.h>

#define EVBU_SIZE 32
#define DEFAULT_POLL_INTERVAL 500 // 0.5 second

static char evbu_data[EVBU_SIZE];
static unsigned int poll_interval = DEFAULT_POLL_INTERVAL;
static spinlock_t data_lock;
static struct kobject *evbu_kobj;
static struct delayed_work poll_work;

// Функция для вызова ACPI-метода EV20
static int call_ev20(void) {
    acpi_status status;
    struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };
    union acpi_object *obj;
    unsigned long flags;

    status = acpi_evaluate_object(NULL, "\\_SB_.PCI0.WMID.EV20", NULL, &buffer);
    if (ACPI_FAILURE(status)) {
        pr_err("Failed to evaluate EV20\n");
        return -ENODEV;
    }

    obj = buffer.pointer;
    if (obj && obj->type == ACPI_TYPE_BUFFER && obj->buffer.length >= EVBU_SIZE) {
        spin_lock_irqsave(&data_lock, flags);
        memcpy(evbu_data, obj->buffer.pointer, EVBU_SIZE);
        spin_unlock_irqrestore(&data_lock, flags);
    } else {
        pr_err("Invalid EVBU data\n");
        kfree(buffer.pointer);
        return -EINVAL;
    }

    kfree(buffer.pointer);
    return 0;
}

// Workqueue функция для периодического опроса
static void poll_evbu(struct work_struct *work) {
    if (call_ev20())
        pr_warn("Error polling EVBU data\n");
    
    schedule_delayed_work(&poll_work, msecs_to_jiffies(poll_interval));
}

// Sysfs: Показать данные
static ssize_t data_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    unsigned long flags;
    spin_lock_irqsave(&data_lock, flags);
    memcpy(buf, evbu_data, EVBU_SIZE);
    spin_unlock_irqrestore(&data_lock, flags);
    return EVBU_SIZE;
}

// Sysfs: Показать/установить интервал опроса
static ssize_t interval_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return sprintf(buf, "%u\n", poll_interval);
}

static ssize_t interval_store(struct kobject *kobj, struct kobj_attribute *attr,
                              const char *buf, size_t count) {
    unsigned int val;
    if (kstrtouint(buf, 10, &val))
        return -EINVAL;
    
    poll_interval = val;
    return count;
}

// Атрибуты sysfs
static struct kobj_attribute data_attribute = 
    __ATTR(data, 0444, data_show, NULL);
static struct kobj_attribute interval_attribute = 
    __ATTR(interval, 0644, interval_show, interval_store);

static struct attribute *attrs[] = {
    &data_attribute.attr,
    &interval_attribute.attr,
    NULL,
};

static struct attribute_group attr_group = {
    .attrs = attrs,
};

// Инициализация модуля
static int __init evbu_module_init(void) {
    int ret;

    spin_lock_init(&data_lock);
    
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
    schedule_delayed_work(&poll_work, msecs_to_jiffies(poll_interval));

    return 0;
}

// Выгрузка модуля
static void __exit evbu_module_exit(void) {
    cancel_delayed_work_sync(&poll_work);
    sysfs_remove_group(evbu_kobj, &attr_group);
    kobject_put(evbu_kobj);
    pr_info("Maibenben special keys reader module unloaded\n");
}

module_init(evbu_module_init);
module_exit(evbu_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alexander Svobodov");
MODULE_DESCRIPTION("Maibenben special keys reader Module with SysFS");