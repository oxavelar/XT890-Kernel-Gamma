/*
 * Copyright (c) 2010, Motorola, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/input/touch_platform.h>

#include "board-mmi.h"
#include "device_libs/platform_mmi-sensors.h"

#include <asm/intel-mid.h>
/*
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
*/

static	uint8_t tdat_filename_p2[] = "atmxt-r2.tdat";
static	uint8_t tdat_filename_p1[] = "atmxt-r1.tdat";
static	uint8_t tdat_filename_p05[] = "atmxt-r0.tdat";


struct touch_platform_data ts_platform_data_atmxt;

static u8 ifwi_major;
static u32 atmxt_system_rev;

static int mmi_atmxt_get_oemb(struct sfi_table_header *table)
{
	struct sfi_table_oemb *oemb;

	oemb = (struct sfi_table_oemb *) table;
	if (!oemb)
		return -EINVAL;

	ifwi_major = oemb->ifwi_major_version;
	return 0;
}

static int mmi_atmxt_get_oemr(struct sfi_table_header *table)
{
	struct sfi_table_oemr *oemr;

	oemr = (struct sfi_table_oemr *) table;
	if (!oemr)
		return -EINVAL;

	atmxt_system_rev = oemr->boardvev;
	return 0;
}

void *mot_setup_touch_atmxt(void *info)
{
	int	check_rev;
	struct kobject *properties_kobj = NULL;

	/*
	 * Get the oemb table. Some of the fields will help figure out the
	 * hardware version.
	 */
	ifwi_major = 0;

	sfi_table_parse(SFI_SIG_OEMB, NULL, NULL, mmi_atmxt_get_oemb);
	sfi_table_parse(SFI_SIG_OEMR, NULL, NULL, mmi_atmxt_get_oemr);

	/*
	* This is temporary solution: for the P05 version of the phone, the
	* major ifwi version is 41, for P1 is 42 and we can assume that
	* following version will have larger number. Since only P05 will
	* have 224E and require different configuration, this check will
	* be very specific with hardcoded values
	*/
	if (ifwi_major == 0x41) {
		strcpy(ts_platform_data_atmxt.filename, tdat_filename_p05);
	} else {
		/*
		 * Pick up tdat file for different HW rev
		 * For now, P1 will have id of 0x8100. Anything higher then
		 * that, would be treated as P2 or better
		 */

		check_rev = atmxt_system_rev & 0xFF00;
		if (check_rev <= 0x8100) {
			strcpy(ts_platform_data_atmxt.filename,
					tdat_filename_p1);
		} else {
			strcpy(ts_platform_data_atmxt.filename,
					tdat_filename_p2);
		}
	}

	/* Setup GPIOs */
	ts_platform_data_atmxt.gpio_reset = get_gpio_by_name("ts_rst");
	ts_platform_data_atmxt.gpio_irq = get_gpio_by_name("ts_int");

	return &ts_platform_data_atmxt;
}
