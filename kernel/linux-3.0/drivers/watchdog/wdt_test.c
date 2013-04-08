/*
 * Simple hardware watchdog regression test module.
 *
 * Copyright (C) 2012 Motorola Mobility, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/smp.h>

static int fire_wdt_reset_set(void *data, u64 val)
{
	printk(KERN_WARNING "CPU#%d fires wdt reset.\n",
	       smp_processor_id());
	printk(KERN_WARNING "Please wait ...\n");
	local_irq_disable();
	while (1)
	;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fire_wdt_reset_fops,
			NULL, fire_wdt_reset_set, "%llu\n");

static int init_watchdog_dbg(void)
{
	debugfs_create_file("fire_wdt_reset", 0200,
			    NULL, NULL, &fire_wdt_reset_fops);
	return 0;
}

late_initcall(init_watchdog_dbg);
MODULE_DESCRIPTION("Watchdog Driver Test");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
