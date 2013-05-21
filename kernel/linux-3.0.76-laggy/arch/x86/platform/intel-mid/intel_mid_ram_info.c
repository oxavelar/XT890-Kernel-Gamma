/*
 * Intel mid ram info support
 *
 * Copyright (C) 2012 Motorola Mobility, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/spinlock.h>

#include <asm/intel-mid.h>
#include <asm/apic.h>

#define PCI_ADDR_MCR	0xD0
#define PCI_ADDR_MDR	0xD4

#define MCR_OP_READ	0x10000000
#define MCR_OP_RAM_INIT	0x68000000
#define MCR_PORT_RAM_0	0x00010000
#define MCR_PORT_RAM_1	0x000B0000
#define MCR_ADDR_MRR	0x00000600
#define MCR_MSG_EN_ALL	0x000000F0

#define MDR_PORT_RAM_0	0x00100000
#define MDR_PORT_RAM_1	0x00200000
#define MDR_CMD_MR	0x00000008
#define MDR_MR_MANFID	0x00000050

static struct {
	unsigned mr5;
	unsigned mr6;
	unsigned mr7;
	unsigned mr8;
} ram_info_mr;

static char ram_info_type_name[20] = "unknown";
static char ram_info_vendor_name[20] = "unknown";
static u32 ram_info_ramsize;
static int ram_info_read;

static int ram_info_read_hw(void);

static ssize_t ram_info_mr_register_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	u32 val = 0;
	const char *name = attr->attr.name;

	if (!ram_info_read)
		ram_info_read_hw();

	if (ram_info_ramsize > 0 &&
		strnlen(name, 4) == 3 && name[0] == 'm' && name[1] == 'r') {
		switch (name[2]) {
		case '5':
			val = ram_info_mr.mr5;
			break;
		case '6':
			val = ram_info_mr.mr6;
			break;
		case '7':
			val = ram_info_mr.mr7;
			break;
		case '8':
			val = ram_info_mr.mr8;
			break;
		}
	}

	return snprintf(buf, 6, "0x%02x\n", val);
}

static ssize_t ram_info_size_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	if (!ram_info_read)
		ram_info_read_hw();

	return snprintf(buf, 12, "%u\n", ram_info_ramsize);
}

static ssize_t ram_info_info_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	if (!ram_info_read)
		ram_info_read_hw();

	return snprintf(buf, 60, "%s:%s:%uMB\n",
			ram_info_vendor_name,
			ram_info_type_name,
			ram_info_ramsize);
}

static ssize_t ram_info_type_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	if (!ram_info_read)
		ram_info_read_hw();

	return snprintf(buf, 20, "%s\n", ram_info_type_name);
}

static struct kobj_attribute ddr_mr5_register_attr =
	__ATTR(mr5, 0444, ram_info_mr_register_show, NULL);

static struct kobj_attribute ddr_mr6_register_attr =
	__ATTR(mr6, 0444, ram_info_mr_register_show, NULL);

static struct kobj_attribute ddr_mr7_register_attr =
	__ATTR(mr7, 0444, ram_info_mr_register_show, NULL);

static struct kobj_attribute ddr_mr8_register_attr =
	__ATTR(mr8, 0444, ram_info_mr_register_show, NULL);

static struct kobj_attribute ddr_size_attr =
	__ATTR(size, 0444, ram_info_size_show, NULL);

static struct kobj_attribute ddr_type_attr =
	__ATTR(type, 0444, ram_info_type_show, NULL);

static struct kobj_attribute ddr_info_attr =
	__ATTR(info, 0444, ram_info_info_show, NULL);

static struct attribute *ram_info_properties_attrs[] = {
	&ddr_mr5_register_attr.attr,
	&ddr_mr6_register_attr.attr,
	&ddr_mr7_register_attr.attr,
	&ddr_mr8_register_attr.attr,
	&ddr_size_attr.attr,
	&ddr_type_attr.attr,
	&ddr_info_attr.attr,
	NULL
};

static struct attribute_group ram_info_properties_attr_group = {
	.attrs = ram_info_properties_attrs,
};

static int north_msg_read(u32 cmd, u32 *data)
{
	struct pci_dev *pci_root;
	int rc;

	pci_root = pci_get_bus_and_slot(0, 0);

	rc = pci_write_config_dword(pci_root, PCI_ADDR_MCR, cmd);
	if (rc)
		goto out;

	pci_read_config_dword(pci_root, PCI_ADDR_MDR, data);

out:
	pci_dev_put(pci_root);

	return rc;
}

static int north_msg_write(u32 cmd, u32 data)
{
	struct pci_dev *pci_root;
	int rc;

	pci_root = pci_get_bus_and_slot(0, 0);

	rc = pci_write_config_dword(pci_root, PCI_ADDR_MDR, data);
	if (rc)
		goto out;

	pci_write_config_dword(pci_root, PCI_ADDR_MCR, cmd);

out:
	pci_dev_put(pci_root);

	return rc;
}

static int ram_info_mrr(unsigned mr, unsigned *value)
{
	u32 mcr, mdr;
	int rc;

	mcr = MCR_OP_RAM_INIT | MCR_PORT_RAM_0 | MCR_MSG_EN_ALL;
	mdr = MDR_PORT_RAM_0 | (mr << 4) | MDR_CMD_MR;
	rc = north_msg_write(mcr, mdr);
	if (rc == 0) {
		u32 mrr_data;
		mcr = MCR_OP_READ | MCR_PORT_RAM_0 | MCR_ADDR_MRR |
							MCR_MSG_EN_ALL;
		rc = north_msg_read(mcr, &mrr_data);
		pr_debug("%s: MR%d: 0x%02X\n", __func__, mr, mrr_data);
		if (rc == 0)
			*value = mrr_data;
	}

	return rc;
}
static int ram_info_read_hw(void)
{
	int rc = 0;
	const char *tname = "unknown";
	const char *vname = "unknown";
	static const char *vendors[] = {
		"unknown",
		"Samsung",
		"Qimonda",
		"Elpida",
		"Etron",
		"Nanya",
		"Hynix",
		"Mosel",
		"Winbond",
		"ESMT",
		"unknown",
		"Spansion",
		"SST",
		"ZMOS",
		"Intel"
	};
	static const char *types[] = {
		"S4 SDRAM",
		"S2 SDRAM",
		"N NVM",
		"Reserved"
	};

	rc = ram_info_mrr(5, &ram_info_mr.mr5);
	if (rc == 0)
		rc = ram_info_mrr(6, &ram_info_mr.mr6);
	if (rc == 0)
		rc = ram_info_mrr(7, &ram_info_mr.mr7);
	if (rc == 0)
		rc = ram_info_mrr(8, &ram_info_mr.mr8);
	if (rc == 0) {
		u32 vid, tid, density;
		char apanic_annotation[128];

		/* identify vendor */
		vid = ram_info_mr.mr5 & 0xFF;
		if (vid < (sizeof(vendors)/sizeof(vendors[0])))
			vname = vendors[vid];
		else if (vid == 0xFE)
			vname = "Numonyx";
		else if (vid == 0xFF)
			vname = "Micron";

		snprintf(ram_info_vendor_name, sizeof(ram_info_vendor_name),
			"%s", vname);

		/* identify type */
		tid = ram_info_mr.mr8 & 0x03;
		if (tid < (sizeof(types)/sizeof(types[0])))
			tname = types[tid];

		snprintf(ram_info_type_name, sizeof(ram_info_type_name),
			"%s", tname);

		/* extract size and multiply by 2 (there are two ranks) */
		density = (ram_info_mr.mr8 >> 2) & 0xF;
		ram_info_ramsize = 0x8 << density;
		ram_info_ramsize *= 2;

		snprintf(apanic_annotation, sizeof(apanic_annotation),
			"RAM: %s, %s, %u MB, MR5:0x%02X, MR6:0x%02X, "
			"MR7:0x%02X, MR8:0x%02X\n",
			vname, tname, ram_info_ramsize,
			ram_info_mr.mr5, ram_info_mr.mr6,
			ram_info_mr.mr7, ram_info_mr.mr8);
		pr_info("%s: %s", __func__, apanic_annotation);
/* FIXME:	apanic_mmc_annotate(apanic_annotation);*/
	} else
		pr_err("%s: failed to access DDR mode registers\n", __func__);

	ram_info_read = 1;

	return rc;
}

int __init ram_info_init(void)
{
	int rc = 0;
	static struct kobject *ram_info_properties_kobj;

	ram_info_read = 0;

	/* create sysfs object */
	ram_info_properties_kobj = kobject_create_and_add("ram", NULL);

	if (ram_info_properties_kobj)
		rc = sysfs_create_group(ram_info_properties_kobj,
			&ram_info_properties_attr_group);

	if (!ram_info_properties_kobj || rc)
		pr_err("%s: failed to create /sys/ram\n", __func__);

	return 0;
}
late_initcall(ram_info_init);
