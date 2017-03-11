/*
 * linux/include/asm/bootinfo.h:  Include file for boot information
 *                                provided on Motorola phones
 *
 * Copyright (C) 2012 Motorola, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Date         Author          Comment
 * 05/08/2012   Motorola        Intel platform initial version
 * 01/07/2009   Motorola        Initial version
 */

#ifndef __ASMARM_BOOTINFO_H
#define __ASMARM_BOOTINFO_H


#if !defined(__KERNEL__) || defined(CONFIG_BOOTINFO)

/*
 * These #defines are used for the bits in powerup_reason.
 */
#define PU_REASON_USB_CABLE             0x00000010 /* Bit 4  */
#define PU_REASON_FACTORY_CABLE         0x00000020 /* Bit 5  */
#define PU_REASON_PWR_KEY_PRESS         0x00000080 /* Bit 7  */
#define PU_REASON_CHARGER               0x00000100 /* Bit 8  */
#define PU_REASON_POWER_CUT             0x00000200 /* bit 9  */
#define PU_REASON_SW_AP_RESET           0x00004000 /* Bit 14 */
#define PU_REASON_WDOG_AP_RESET         0x00008000 /* Bit 15 */
#define PU_REASON_AP_KERNEL_PANIC       0x00020000 /* Bit 17 */
#define PU_REASON_HW_RESET		0x00100000 /* Bit 20 */

/*
   bootmode reasons as defined by Intel
   */

#define I_PWR_RS_BATT_INSERT		0x0
#define	I_PWR_RS_PWR_BUTTON_PRESS	0x1
#define	I_PWR_RS_RTC_TIMER			0x2
#define I_PWR_RS_USB_CHRG_INSERT	0x3
#define	I_PWR_RS_RESERVED			0x4
#define	I_PWR_RS_COLD_RESET			0x5
#define	I_PWR_RS_COLD_BOOT			0x6
#define	I_PWR_RS_UNKNOWN			0x7
#define	I_PWR_SWWDT_RESET			0x8
#define	I_PWR_HWWDT_RESET			0x9

/*
 * These #defines are used for the battery status at boot.
 * When no battery is present, the status is BATTERY_LO_VOLTAGE.
 */
#define BATTERY_GOOD_VOLTAGE    1
#define BATTERY_LO_VOLTAGE      2
#define BATTERY_UNKNOWN         (-1)

/*
 * /proc/bootinfo has a strict format.  Each line contains a name/value
 * pair which are separated with a colon and a single space on both
 * sides of the colon.  The following defines help you size the
 * buffers used to read the data from /proc/bootinfo.
 *
 * BOOTINFO_MAX_NAME_LEN:  maximum size in bytes of a name in the
 *                         bootinfo line.  Don't forget to add space
 *                         for the NUL if you need it.
 * BOOTINFO_MAX_VAL_LEN:   maximum size in bytes of a value in the
 *                         bootinfo line.  Don't forget to add space
 *                         for the NUL if you need it.
 * BOOTINFO_BUF_SIZE:      size in bytes of buffer that is large enough
 *                         to read a /proc/bootinfo line.  The extra
 *                         3 is for the " : ".  Don't forget to add
 *                         space for the NUL and newline if you
 *                         need them.
 */
#define BOOTINFO_MAX_NAME_LEN    32
#define BOOTINFO_MAX_VAL_LEN    128
#define BOOTINFO_BUF_SIZE       (BOOTINFO_MAX_NAME_LEN + \
					3 + BOOTINFO_MAX_VAL_LEN)

#endif


#if defined(__KERNEL__) && defined(CONFIG_BOOTINFO)

extern struct proc_dir_entry proc_root;

u32  bi_powerup_reason(void);
void bi_set_powerup_reason(u32 powerup_reason);

u32  bi_ifw_version(void);
void bi_set_ifw_version(u32 ifw_version);

u32  bi_bos_version(void);
void bi_set_bos_version(u32 bos_version);

u32  bi_battery_status_at_boot(void);
void bi_set_battery_status_at_boot(u32 battery_status_at_boot);

u8  bi_cid_recover_boot(void);
void bi_set_cid_recover_boot(u8 cid_recover_boot);

#endif /* defined(__KERNEL__) && defined(CONFIG_BOOTINFO) */

#endif
