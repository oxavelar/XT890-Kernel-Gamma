/*
 * bootinfo.c: This file contains the bootinfo code.  This code
 *	  provides boot information via /proc/bootinfo.  The
 *	  information currently includes:
 *            the powerup reason
 *        This file also provides EZX compatible interfaces for
 *	  retrieving the powerup reason.  All new user-space consumers
 *	  of the powerup reason should use the /proc/bootinfo
 *	  interface and all kernel-space consumers of the powerup
 *	  reason should use the bi_powerup_reason interface.  The EZX
 *	  compatibility code is deprecated.
 *
 *
 * Copyright (C) 2012 Motorola, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
 * Revision History:
 *
 * Date         Author    Comment
 * ----------   --------  -----------
 * 05/08/2012   Motorola  Intel platform initialize version
 * 30/06/2009   Motorola  Initialize version
 */


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <asm/setup.h>
#include <asm/bootinfo.h>
#include <asm/intel_scu_ipc.h>
#include <linux/notifier.h>
#include <asm/tsc.h>

#define MAX_VERSION_LENGTH  32
#define MAX_WAKESRC_LENGTH  16
static int local_htoi(const char *name);
static char optstr[MAX_VERSION_LENGTH];
static char woptstr[MAX_WAKESRC_LENGTH];
/*
 * EMIT_BOOTINFO and EMIT_BOOTINFO_STR are used to emit the bootinfo
 * information for data provided via ATAGs.
 *
 * The format for all bootinfo lines is "name : val" and these macros
 * enforce that format.
 *
 * strname is the name printed in the name/val pair.
 * name is the name of the function to call
 *
 * EMIT_BOOTINFO and EMIT_BOOTINFO_STR depend on buf and len to already
 * be defined.
 */
#define EMIT_BOOTINFO(strname, fmt, name) \
		do { \
			len += sprintf(buf+len, strname " : " fmt "\n", \
					bi_##name()); \
		} while (0)

#define EMIT_BOOTINFO_STR(strname, name) \
		do { \
			unsigned char *ptr; \
			ptr = (unsigned char *)bi_##name(); \
			if (strlen(ptr) == 0) { \
				len += sprintf(buf+len, strname \
							" : UNKNOWN\n"); \
			} else { \
				len += sprintf(buf+len, strname \
							" : %s\n", ptr); \
			} \
		} while (0)

/*-------------------------------------------------------------------------*/

/*
 * powerup_reason contains the powerup reason provided by the ATAGs when
 * the machine boots.
 *
 * Exported symbols:
 * bi_powerup_reason()             -- returns the powerup reason
 * bi_set_powerup_reason()         -- sets the powerup reason
 */
static u32 powerup_reason;

u32 bi_powerup_reason(void)
{
	powerup_reason = local_htoi(woptstr);

	return powerup_reason;
}
EXPORT_SYMBOL(bi_powerup_reason);

void bi_set_powerup_reason(u32 __powerup_reason)
{
	powerup_reason = __powerup_reason;
}
EXPORT_SYMBOL(bi_set_powerup_reason);

#define EMIT_POWERUPREASON() \
	    EMIT_BOOTINFO("POWERUPREASON", "0x%08x", powerup_reason)

struct scu_ipc_version {
	u32     count;  /* length of version info */
	u8      data[16]; /* version data */
};


/*
 * ifw_version contains the Intel FW version.
 * scu_version contains the BootOS  version.
 * ifw_version and scu_version default to 0 if they are
 * not set.
 *
 * Exported symbols:
 * bi_ifw_version()                -- returns the IFW version
 * bi_set_ifw_version()            -- sets the IFW version
 * bi_scu_version()                -- returns the SCU version
 * bi_set_scu_version()            -- sets the SCU version
 * bi_bos_version()                -- returns the BOS version
 * bi_set_bos_version()            -- sets the BOS version
 */
static u32 ifw_version;
u32 bi_ifw_version(void)
{
	int ret = 0;
	struct scu_ipc_version version;

	version.count = 16;

	ret = intel_scu_ipc_command(IPCMSG_FW_REVISION, 0, NULL, 0,
				(u32 *)version.data, 4);
	if (ret < 0) {
		printk(KERN_ERR "%s[%d] ret=0x%08x ", __FILE__, __LINE__, ret);
		return (u32)ret;
	}

	ifw_version = version.data[15];
	ifw_version = (ifw_version << 8) + version.data[14];

	return ifw_version;
}
EXPORT_SYMBOL(bi_ifw_version);

void bi_set_ifw_version(u32 __ifw_version)
{
	ifw_version = __ifw_version;
}
EXPORT_SYMBOL(bi_set_ifw_version);

static u32 scu_version;
u32 bi_scu_version(void)
{
	int ret = 0;
	u32 fw_version;


	ret = intel_scu_ipc_command(IPCMSG_FW_REVISION, 0, NULL, 0,
			&fw_version, 1);
	if (ret < 0) {
		printk(KERN_ERR "%s[%d] ret=0x%08x ", __FILE__, __LINE__, ret);
		return (u32)ret;
	}

	scu_version = fw_version;

	return scu_version;
}
EXPORT_SYMBOL(bi_scu_version);

void bi_set_scu_version(u32 __scu_version)
{
	scu_version = __scu_version;
}
EXPORT_SYMBOL(bi_set_scu_version);

static int local_htoi(const char *name)
{
	int val = 0;

	for (;; name++) {
		switch (*name) {
		case '0' ... '9':
			if (*name == '0' && (*(name+1) == 'x'
						|| *(name+1) == 'X')) {
				name++;
				break;
			}
			val = 16*val+(*name-'0');
		break;
		case 'a' ... 'f':
			val = 16*val+((*name-'a')+10);
		break;
		case 'A' ... 'F':
			val = 16*val+((*name-'A')+10);
		break;
		default:
			return val;
		}
	}
}

static u32 bos_version;

u32 bi_bos_version(void)
{
	bos_version = local_htoi(optstr);

	return bos_version;
}
EXPORT_SYMBOL(bi_bos_version);

static int __init get_dr_ver_optstr(char *str)
{
	memset(optstr, 0, MAX_VERSION_LENGTH);
	memcpy(optstr, str, MAX_VERSION_LENGTH - 1);

	return 1;
}
static int __init get_dr_wksrc_optstr(char *str)
{
	memset(woptstr, 0, MAX_WAKESRC_LENGTH);
	memcpy(woptstr, str, MAX_WAKESRC_LENGTH - 1);

	return 1;
}
__setup("androidboot.bootloader=", get_dr_ver_optstr);
__setup("androidboot.wakesrc=", get_dr_wksrc_optstr);


void bi_set_bos_version(u32 __bos_version)
{
	bos_version = __bos_version;
}
EXPORT_SYMBOL(bi_set_bos_version);

#define EMIT_IFW_VERSION() \
		EMIT_BOOTINFO("IFW_VERSION", "0x%08x", ifw_version)
#define EMIT_SCU_VERSION() \
		EMIT_BOOTINFO("SCU_VERSION", "0x%08x", scu_version)
#define EMIT_BOS_VERSION() \
		EMIT_BOOTINFO("MBM_VERSION", "0x%08x", bos_version)

#define IPC_CMD_CC_RD         1


/*
 * battery_status_at_boot indicates the battery status
 * when the machine started to boot.
 * battery_status_at_boot defaults to -1 (0xffff) if the battery
 * status can't be determined.
 *
 * Exported symbols:
 * bi_battery_status_at_boot()         -- returns the battery boot status
 * bi_set_battery_status_at_boot()     -- sets the battery boot status
 */
static u32 battery_status_at_boot = -1;

u32 bi_battery_status_at_boot(void)
{
	battery_status_at_boot = 0;

	return battery_status_at_boot;
}
EXPORT_SYMBOL(bi_battery_status_at_boot);

void bi_set_battery_status_at_boot(u32 __battery_status_at_boot)
{
	battery_status_at_boot = __battery_status_at_boot;
}
EXPORT_SYMBOL(bi_set_battery_status_at_boot);

#define EMIT_BATTERY_STATUS_AT_BOOT() \
		EMIT_BOOTINFO("BATTERY_STATUS_AT_BOOT", "0x%04x", \
					battery_status_at_boot)

/*
 * cid_recover_boot contains the flag to indicate whether phone should
 * boot into recover mode or not.
 * cid_recover_boot defaults to 0 if it is not set.
 *
 * Exported symbols:
 * bi_cid_recover_boot()        -- returns the value of recover boot
 * bi_set_cid_recover_boot()    -- sets the value of recover boot
 */
static u8 cid_recover_boot;
u8 bi_cid_recover_boot(void)
{
	return cid_recover_boot;
}
EXPORT_SYMBOL(bi_cid_recover_boot);

void bi_set_cid_recover_boot(u8 __cid_recover_boot)
{
	cid_recover_boot = __cid_recover_boot;
}
EXPORT_SYMBOL(bi_set_cid_recover_boot);


#define EMIT_CID_RECOVER_BOOT() \
		EMIT_BOOTINFO("CID_RECOVER_BOOT", "0x%02x", cid_recover_boot)

#ifdef CONFIG_BOARD_MFLD_MOTO

extern u32 system_rev;

static u32 bi_hwrev(void)
{
	return system_rev;
}

#define EMIT_HWREV() EMIT_BOOTINFO("HW_REV", "0x%08x", hwrev)

#else
#define EMIT_HWREV() do { } while (0)
#endif

/*
 * get_bootinfo fills in the /proc/bootinfo information.
 * We currently only have the powerup reason.
 */
static int get_bootinfo(char *buf, char **start,
						off_t offset, int count,
						int *eof, void *data)
{
	int len = 0;

	EMIT_HWREV();
	EMIT_POWERUPREASON();
	EMIT_BOS_VERSION();
	EMIT_IFW_VERSION();
	EMIT_SCU_VERSION();
	EMIT_BATTERY_STATUS_AT_BOOT();
	EMIT_CID_RECOVER_BOOT();

	return len;
}

static int bootinfo_panic(struct notifier_block *this,
						unsigned long event, void *ptr)
{
	printk(KERN_ERR "ifw_version=0x%08x", ifw_version);
	printk(KERN_ERR "scu_version=0x%08x", scu_version);
	printk(KERN_ERR "\ncpu frequency= %lu.%03lu MHz\n",
			(unsigned long)cpu_khz / 1000,
			(unsigned long)cpu_khz % 1000);
	return NOTIFY_DONE;
}

static struct notifier_block panic_block = {
	.notifier_call = bootinfo_panic,
	.priority = INT_MAX,
};

static struct proc_dir_entry *proc_bootinfo;

int __init bootinfo_init_module(void)
{
	proc_bootinfo = &proc_root;
	create_proc_read_entry("bootinfo", 0, NULL, get_bootinfo, NULL);
	atomic_notifier_chain_register(&panic_notifier_list, &panic_block);
	return 0;
}

void __exit bootinfo_cleanup_module(void)
{
	if (proc_bootinfo) {
		remove_proc_entry("bootinfo", proc_bootinfo);
		proc_bootinfo = NULL;
	}
}

module_init(bootinfo_init_module);
module_exit(bootinfo_cleanup_module);

MODULE_AUTHOR("MOTOROLA");
