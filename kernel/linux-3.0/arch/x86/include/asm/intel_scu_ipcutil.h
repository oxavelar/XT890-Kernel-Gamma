#ifndef _ASM_X86_INTEL_SCU_IPCUTIL_H_
#define _ASM_X86_INTEL_SCU_IPCUTIL_H_

#include <linux/types.h>

/* ioctl commnds */
#define INTEL_SCU_IPC_REGISTER_READ	0
#define INTEL_SCU_IPC_REGISTER_WRITE	1
#define INTEL_SCU_IPC_REGISTER_UPDATE	2
#define INTEL_SCU_IPC_FW_UPDATE		0xA2
#define INTEL_SCU_IPC_MEDFIELD_FW_UPDATE	0xA3
#define INTEL_SCU_IPC_FW_REVISION_GET	0xB0
#define INTEL_SCU_IPC_S0IX_RESIDENCY	0xB8
#define INTEL_SCU_IPC_READ_RR_FROM_OSNIB	0xC1
#define INTEL_SCU_IPC_WRITE_RR_TO_OSNIB	0xC2
#define INTEL_SCU_IPC_READ_VBATTCRIT	0xC4
#define INTEL_SCU_IPC_WRITE_ALARM_FLAG_TO_OSNIB	0xC5
#define INTEL_SCU_IPC_OSC_CLK_CNTL	0xC6
#define INTEL_SCU_IPC_READ_BYTE_IN_OSNIB	0xD0
#define INTEL_SCU_IPC_WRITE_BYTE_IN_OSNIB	0xD1
#define INTEL_SCU_IPC_READ_BOS_FROM_OSNIB	0xD2
#define INTEL_SCU_IPC_WRITE_BOS_TO_OSNIB	0xD3
#define INTEL_SCU_IPC_READ_FBMODE_FROM_OSNIB	0xD4
#define INTEL_SCU_IPC_WRITE_FBMODE_TO_OSNIB	0xD5

struct scu_ipc_data {
	__u32	count;  /* No. of registers */
	__u16	addr[5]; /* Register addresses */
	__u8	data[5]; /* Register data */
	__u8	mask; /* Valid for read-modify-write */
};

struct scu_ipc_version {
	__u32	count;  /* length of version info */
	__u8	data[16]; /* version data */
};

struct osc_clk_t {
	__u32	id; /* clock id */
	__u32	khz; /* clock frequency */
};

struct osnib_data {
	u8 offset;
	u8 data;
};

/* Penwell has 4 osc clocks */
#define OSC_CLK_AUDIO	0	/* Audio */
#define OSC_CLK_CAM0	1	/* Primary camera */
#define OSC_CLK_CAM1	2	/* Secondary camera */
#define OSC_CLK_DISP	3	/* Display buffer */

#ifdef __KERNEL__

int intel_scu_ipc_osc_clk(u8 clk, unsigned int khz);

enum clk0_mode {
	CLK0_AUDIENCE = 0x4,
	CLK0_VIBRA1 = 0x8,
	CLK0_VIBRA2 = 0x10,
	CLK0_MSIC = 0x20,
	CLK0_HSJACK = 0x40,
	CLK0_DEBUG = 0x100,
	CLK0_QUERY = 0x1000,
};

int intel_scu_ipc_set_osc_clk0(unsigned int enable, enum clk0_mode mode);

/* Helpers to turn on/off msic vprog1 and vprog2 */
int intel_scu_ipc_msic_vprog1(int on);
int intel_scu_ipc_msic_vprog2(int on);

#define OSHOB_SCU_TRACE_OFFSET		0x00
#define OSHOB_IA_TRACE_OFFSET		0x04
#define OSHOB_PMIT_OFFSET		0x0000002c
#define PMIT_RESETIRQ1_OFFSET		14
#define PMIT_RESETIRQ2_OFFSET		15
/* OSHOB-OS Handoff Buffer read */
int intel_scu_ipc_read_oshob(u8 *data, int len, int offset);

/*
 * OSHOB offset 0x0c is the address for osNoInitData, totally 32 Bytes.
 * This area is used for IA f/w when bootup.
 * BYTE[0] : RR, reboot reason. As you have get all the answer now.
 * BYTE[1] : WDT, watch dog timer indication, set BIT0.
 * BYTE[2] : RTC, RTC timer indication, set BIT0.
 * BYTE[3] : wake src, as we have listed before for all possibilities.
 * BYTE[4] : RstIrq1, used for MSIC reset IRQ1
 * BYTE[5] : RstIrq2, used for MSIC reset IRQ2
 * BYTE[6]~BYTE[30]: Reserved.
 * BYTE[31]: Checksum.
 *
 * Motorola uses BYTE[28] as fastboot mode flag, set BIT0
 * Motorola uses BYTE[29] as primary and backup bso sync flag, set BIT0
 * Motorola uses BYTE[30] as panic, HW reset and normal AP reset power up reason
 */
#define OSNIB_OFFSET			0x0C
#define OSNIB_RR_OFFSET			0
#define OSNIB_WD_OFFSET			1
#define OSNIB_ALARM_OFFSET		2
#define OSNIB_WAKESRC_OFFSET		3
#define OSNIB_RESETIRQ1_OFFSET		4
#define OSNIB_RESETIRQ2_OFFSET		5
#define OSNIB_FBMODE_OFFSET		28
#define OSNIB_BOS_OFFSET		29
#define OSNIB_PANIC_OFFSET		30

/* OSNIB BYTE[30] bits definition */
#define OSNIB_PU_REASON_SW_AP_RESET	0x01
#define OSNIB_PU_REASON_HW_RESET	0x00
#define OSNIB_PU_REASON_AP_KERNEL_PANIC	0x02

/* OSNIB-OS No Init Buffer write */
int intel_scu_ipc_write_osnib(u8 *data, int len, int offset);
int intel_scu_ipc_read_osnib(u8 *data, int len, int offset);
int intel_scu_ipc_write_osnib_rr(u8 rr);
int intel_scu_ipc_read_osnib_rr(u8 *rr);
int intel_scu_ipc_write_osnib_byte(u8 offset, u8 data);
int intel_scu_ipc_read_osnib_byte(u8 offset, u8 *data);
int intel_scu_ipc_write_osnib_bos(u8 bos);
int intel_scu_ipc_read_osnib_bos(u8 *bos);
int intel_scu_ipc_write_osnib_fb_mode(u8 bos);
int intel_scu_ipc_read_osnib_fb_mode(u8 *bos);

#endif

#endif
