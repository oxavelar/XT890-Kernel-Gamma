/*
 * Support for SEMCO SH833-SU HVCA focuser.
 *
 * Copyright (c) 2012 Motorola Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kmod.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include <linux/atomisp_platform.h>
#include <linux/atomisp.h>

#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <asm/intel_scu_ipc.h>


/* ---------------------------------------------------------------------- */
/* Internal definitions */

#define	SH833SU_NAME					"sh833su"

#define SH833SU_MAX_FOCUS_POS				255
#define SH833SU_MAX_FOCUS_NEG				(-255)

#define SH833SU_SOFT_FOCUS_MACRO			76
#define SH833SU_SOFT_FOCUS_INF				213

#define SH833SU_STATUS_MOVING				0x1
#define SH833SU_STATUS_COMPLETE				0x0

#define SH833SU_EEPROM_ADDRESS				0x50
#define SH833SU_EEPROM_BUFFER_SIZE			512

/* EEPROM read and probe attempts */
#define SH833SU_INIT_ATTEMPTS				25

#define SH833SU_EEPROM_MAP_INF1				0x20
#define SH833SU_EEPROM_MAP_MAC1				0x22
#define SH833SU_EEPROM_MAP_INF2				0x24
#define SH833SU_EEPROM_MAP_MAC2				0x26

/* EEPROM Initialization Block */
#define HVCA_DATA_1BYTE					0x01
#define HVCA_DATA_2BYTE					0x02

#define HVCA_DIRECT_MODE				0x00
#define HVCA_INDIRECT_EEPROM				0x10
#define HVCA_INDIRECT_HVCA				0x20
#define HVCA_MASK_AND					0x70
#define HVCA_MASK_OR					0x80

/* Convergence judgment */
#define INI_MSSET_211					0x00
#define CHTGOKN_TIME					0x80
#define CHTGOKN_WAIT					1
#define CHTGOKN_TIMEOUT					50
#define CHTGSTOKN_TOMEOUT				15

/* Step move */
#define STMV_SIZE					0x0180

#define STMCHTG_ON					0x08
#define STMSV_ON					0x04
#define STMLFF_ON					0x02
#define STMVEN_ON					0x01
#define STMCHTG_OFF					0x00
#define STMSV_OFF					0x00
#define STMLFF_OFF					0x00
#define STMVEN_OFF					0x00

#define STMCHTG_SET					STMCHTG_ON
#define STMSV_SET					STMSV_ON
#define STMLFF_SET					STMLFF_OFF

#define CHTGST_ON					0x01
#define DEFAULT_DADAT					0x8040

/* Delay RAM 00h ~ 3Fh */
#define ADHXI_211H					0x00
#define ADHXI_211L					0x01
#define PIDZO_211H					0x02
#define PIDZO_211L					0x03
#define RZ_211H						0x04
#define RZ_211L						0x05
#define DZ1_211H					0x06
#define DZ1_211L					0x07
#define DZ2_211H					0x08
#define DZ2_211L					0x09
#define UZ1_211H					0x0A
#define UZ1_211L					0x0B
#define UZ2_211H					0x0C
#define UZ2_211L					0x0D
#define IZ1_211H					0x0E
#define IZ1_211L					0x0F
#define IZ2_211H					0x10
#define IZ2_211L					0x11
#define MS1Z01_211H					0x12
#define MS1Z01_211L					0x13
#define MS1Z11_211H					0x14
#define MS1Z11_211L					0x15
#define MS1Z12_211H					0x16
#define MS1Z12_211L					0x17
#define MS1Z22_211H					0x18
#define MS1Z22_211L					0x19
#define MS2Z01_211H					0x1A
#define MS2Z01_211L					0x1B
#define MS2Z11_211H					0x1C
#define MS2Z11_211L					0x1D
#define MS2Z12_211H					0x1E
#define MS2Z12_211L					0x1F
#define MS2Z22_211H					0x20
#define MS2Z22_211L					0x21
#define MS2Z23_211H					0x22
#define MS2Z23_211L					0x23
#define OZ1_211H					0x24
#define OZ1_211L					0x25
#define OZ2_211H					0x26
#define OZ2_211L					0x27
#define DAHLXO_211H					0x28
#define DAHLXO_211L					0x29
#define OZ3_211H					0x2A
#define OZ3_211L					0x2B
#define OZ4_211H					0x2C
#define OZ4_211L					0x2D
#define OZ5_211H					0x2E
#define OZ5_211L					0x2F
#define oe_211H						0x30
#define oe_211L						0x31
#define MSR1CMAX_211H					0x32
#define MSR1CMAX_211L					0x33
#define MSR1CMIN_211H					0x34
#define MSR1CMIN_211L					0x35
#define MSR2CMAX_211H					0x36
#define MSR2CMAX_211L					0x37
#define MSR2CMIN_211H					0x38
#define MSR2CMIN_211L					0x39
#define OFFSET_211H					0x3A
#define OFFSET_211L					0x3B
#define ADOFFSET_211H					0x3C
#define ADOFFSET_211L					0x3D
#define EZ_211H						0x3E
#define EZ_211L						0x3F

/* Coefficient RAM 40h ~ 7Fh */
#define AG_211H						0x40
#define AG_211L						0x41
#define DA_211H						0x42
#define DA_211L						0x43
#define DB_211H						0x44
#define DB_211L						0x45
#define DC_211H						0x46
#define DC_211L						0x47
#define DG_211H						0x48
#define DG_211L						0x49
#define PG_211H						0x4A
#define PG_211L						0x4B
#define GAIN1_211H					0x4C
#define GAIN1_211L					0x4D
#define GAIN2_211H					0x4E
#define GAIN2_211L					0x4F
#define UA_211H						0x50
#define UA_211L						0x51
#define UC_211H						0x52
#define UC_211L						0x53
#define IA_211H						0x54
#define IA_211L						0x55
#define IB_211H						0x56
#define IB_211L						0x57
#define I_C_211H					0x58
#define I_C_211L					0x59
#define MS11A_211H					0x5A
#define MS11A_211L					0x5B
#define MS11C_211H					0x5C
#define MS11C_211L					0x5D
#define MS12A_211H					0x5E
#define MS12A_211L					0x5F
#define MS12C_211H					0x60
#define MS12C_211L					0x61
#define MS21A_211H					0x62
#define MS21A_211L					0x63
#define MS21B_211H					0x64
#define MS21B_211L					0x65
#define MS21C_211H					0x66
#define MS21C_211L					0x67
#define MS22A_211H					0x68
#define MS22A_211L					0x69
#define MS22C_211H					0x6A
#define MS22C_211L					0x6B
#define MS22D_211H					0x6C
#define MS22D_211L					0x6D
#define MS22E_211H					0x6E
#define MS22E_211L					0x6F
#define MS23P_211H					0x70
#define MS23P_211L					0x71
#define OA_211H						0x72
#define OA_211L						0x73
#define OC_211H						0x74
#define OC_211L						0x75
#define PX12_211H					0x76
#define PX12_211L					0x77
#define PX3_211H					0x78
#define PX3_211L					0x79
#define MS2X_211H					0x7A
#define MS2X_211L					0x7B
#define CHTGX_211H					0x7C
#define CHTGX_211L					0x7D
#define CHTGN_211H					0x7E
#define CHTGN_211L					0x7F

/* Register 80h ~  9F */
#define CLKSEL_211					0x80
#define ADSET_211					0x81
#define PWMSEL_211					0x82
#define SWTCH_211					0x83
#define STBY_211					0x84
#define CLR_211						0x85
#define DSSEL_211					0x86
#define ENBL_211					0x87
#define ANA1_211					0x88
#define STMVEN_211					0x8A
#define STPT_211					0x8B
#define SWFC_211					0x8C
#define SWEN_211					0x8D
#define MSNUM_211					0x8E
#define MSSET_211					0x8F
#define DLYMON_211					0x90
#define MONA_211					0x91
#define PWMLIMIT_211					0x92
#define PINSEL_211					0x93
#define PWMSEL2_211					0x94
#define SFTRST_211					0x95
#define TEST_211					0x96
#define PWMZONE2_211					0x97
#define PWMZONE1_211					0x98
#define PWMZONE0_211					0x99
#define ZONE3_211					0x9A
#define ZONE2_211					0x9B
#define ZONE1_211					0x9C
#define ZONE0_211					0x9D
#define GCTIM_211					0x9E
#define GCTIM_211NU					0x9F
#define STMINT_211					0xA0
#define STMVENDH_211					0xA1
#define STMVENDL_211					0xA2
#define MSNUMR_211					0xA3
#define ANA2_211					0xA4


#define to_sh833su_sensor(sd) container_of(sd, struct sh833su_device, sd)


/* ---------------------------------------------------------------------- */
/* Lens Stroke Information */

/* 2m hyperfocal target, recommended by SEMCO */
#define SH833SU_HYPER_RATIO_2M  (77/2244)

/* 3m hyperfocal target, theoretical best */
#define SH833SU_HYPER_RATIO_3M  (115/2244)

/* MOT empirical best */
#define SH833SU_HYPER_RATIO_MOT (412/2244)



/* ---------------------------------------------------------------------- */
/* Logging section */

#define SH833SU_SANITY_CHECKS
#define SH833SU_LOG_VERBOSE

#define SH833SU_LOG_PREFIX				"sh833su: "

#define sh833su_err(format, args...)	do { \
	printk(KERN_ERR SH833SU_LOG_PREFIX "%s " format, __func__, ##args); \
} while (0)

#define sh833su_info(format, args...)	do { \
	printk(KERN_INFO SH833SU_LOG_PREFIX "%s " format, __func__, ##args); \
} while (0)

#define sh833su_dbg(format, args...)	do { \
	pr_debug(SH833SU_LOG_PREFIX "%s " format, __func__, ##args); \
} while (0)



#ifdef SH833SU_LOG_VERBOSE

#define DUMP_BYTES_PER_LINE		16
#define DUMP_BYTE_CHARS			3


/**
 * Dump memory chunk in usual hex dump format, DUMP_BYTES_PER_LINE chars per line
 *
 * @desc	dump description string
 * @buffer	buffer to dump
 * @length	buffer length
 */
static void sh833su_memory_dump(char *desc, const uint8_t *buffer,
	unsigned length)
{
	static u8 dump[DUMP_BYTES_PER_LINE*DUMP_BYTE_CHARS + 1];
	unsigned i, j;

	pr_debug(SH833SU_LOG_PREFIX "%s (%u bytes):\n", desc, length);

	for (i = 0, j = 0; i < length; ++i) {
		sprintf(dump + (j++)*DUMP_BYTE_CHARS, "%02X ", buffer[i]);

		if (j > DUMP_BYTES_PER_LINE - 1) {
			pr_debug(SH833SU_LOG_PREFIX "%s\n", dump);
			j = 0;
		}
	}

	if (j != 0)
		pr_debug(SH833SU_LOG_PREFIX "%s\n", dump);
}

#else
	#define sh833su_memory_dump(desc, buffer, length) do { } while (0)
#endif /* SH833SU_LOG_VERBOSE */

/* ---------------------------------------------------------------------- */
/* Device context */

/* sh833su device structure
 * Inf2 and Mac2 are the calibration data for SEMCO AF lens.
 * Inf2: Best focus (lens position) when object distance is 1.2M.
 * Mac2: Best focus (lens position) when object distance is 10cm.
 */
struct sh833su_device {
	struct v4l2_subdev sd;

	struct mutex input_lock;		/* serialize actuator's io */
	struct i2c_client *client;		/* reference to I2C client */

#ifdef CONFIG_DEBUG_FS
	struct dentry *sh833su_debugfs_dir;
#endif

	struct camera_sensor_platform_data *platform_data;

	u8 eeprom[SH833SU_EEPROM_BUFFER_SIZE];

	u8 power;
	u8 status;

	s16 focus;

	s16 inf;
	s16 macro;
	s16 hyper;

	u8 manual;
};

struct sh833su_control {
	struct v4l2_queryctrl qc;
	int (*query)(struct v4l2_subdev *sd, s32 *value);
	int (*tweak)(struct v4l2_subdev *sd, s32 value);
};


/* ---------------------------------------------------------------------- */
/* Prototypes */

static int sh833su_power_ctrl(struct sh833su_device *dev, int on);

static int sh833su_hvca_init(struct sh833su_device *dev);
static int sh833su_hvca_set_initial_position(struct sh833su_device *dev);

static int sh833su_lens_move_pulse(struct sh833su_device *dev, s16 target_pos);


/* ---------------------------------------------------------------------- */
/* Internal helpers */
inline s16 sh833su_eeprom_calibration_data(const u8 *buffer, u16 offset)
{
	return (signed)(buffer[offset] << 8 | buffer[offset + 1]);
}

static inline int sh833su_v4l_to_hvca_pos(struct sh833su_device *dev,
		int v4l_focus)
{
	static int soft_inf = SH833SU_SOFT_FOCUS_INF;
	static int soft_macro = SH833SU_SOFT_FOCUS_MACRO;

	return (dev->inf - dev->macro)*(v4l_focus - soft_macro)/
			(soft_inf - soft_macro) + dev->macro;
}


static inline int sh833su_hvca_to_v4l_pos(struct sh833su_device *dev,
		int hvca_focus)
{
	static int soft_inf = SH833SU_SOFT_FOCUS_INF;
	static int soft_macro = SH833SU_SOFT_FOCUS_MACRO;

	return (soft_inf - soft_macro)*(hvca_focus - dev->macro)/
			(dev->inf - dev->macro) + soft_macro;
}


/* ---------------------------------------------------------------------- */
/* Atom ISP sub-device interface */

static int sh833su_t_focus_abs(struct v4l2_subdev *sd, s32 v4l_focus)
{
	struct sh833su_device *dev = to_sh833su_sensor(sd);

	if (dev->manual) {
		sh833su_info("manual focus control enabled; "
				"atomisp detached\n");
		return 0;
	}

	dev->focus = sh833su_v4l_to_hvca_pos(dev, v4l_focus);

	sh833su_dbg("v4l focus = %d, hvca focus = %d\n", v4l_focus, dev->focus);
	return sh833su_lens_move_pulse(dev, dev->focus);
}

static int sh833su_t_focus_rel(struct v4l2_subdev *sd, s32 v4l_delta)
{
	struct sh833su_device *dev = to_sh833su_sensor(sd);
	int v4l_focus = sh833su_hvca_to_v4l_pos(dev, dev->focus) - v4l_delta;
	sh833su_dbg("v4l delta = %d\n", -v4l_delta);

	if (v4l_focus < SH833SU_SOFT_FOCUS_MACRO)
		v4l_focus = SH833SU_SOFT_FOCUS_MACRO;

	else if (v4l_focus > SH833SU_SOFT_FOCUS_INF)
		v4l_focus = SH833SU_SOFT_FOCUS_INF;

	return sh833su_t_focus_abs(sd, v4l_focus);
}

static int sh833su_q_focus_abs(struct v4l2_subdev *sd, s32 *value)
{
	struct sh833su_device *dev = to_sh833su_sensor(sd);

	sh833su_dbg("query focus position\n");
	*value  = sh833su_hvca_to_v4l_pos(dev, dev->focus);

	return 0;
}

static int sh833su_q_focus_status(struct v4l2_subdev *sd, s32 *value)
{
	struct sh833su_device *dev = to_sh833su_sensor(sd);
	u32 status = 0;

	sh833su_dbg("query focus status: 0x%x\n", dev->status);

	if (dev->status) {
		status |= ATOMISP_FOCUS_STATUS_MOVING;
		status |= ATOMISP_FOCUS_HP_IN_PROGRESS;
	} else {
		status |= ATOMISP_FOCUS_STATUS_ACCEPTS_NEW_MOVE;
		status |= ATOMISP_FOCUS_HP_COMPLETE;
	}

	*value = status;

	return 0;
}

/* ---------------------------------------------------------------------- */
/* Atom ISP V4L2 controls */

struct sh833su_control sh833su_controls[] = {
	{
		.qc = {
			.id = V4L2_CID_FOCUS_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focus move absolute",
			.minimum = 0,
			.maximum = SH833SU_MAX_FOCUS_POS,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.tweak = sh833su_t_focus_abs,
		.query = sh833su_q_focus_abs,
	},
	{
		.qc = {
			.id = V4L2_CID_FOCUS_RELATIVE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focus move relative",
			.minimum = SH833SU_MAX_FOCUS_NEG,
			.maximum = SH833SU_MAX_FOCUS_POS,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.tweak = sh833su_t_focus_rel,
	},
	{
		.qc = {
			.id = V4L2_CID_FOCUS_STATUS,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focus status",
			.minimum = 0,
			.maximum = 100, /* allow enum to grow in the future */
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.query = sh833su_q_focus_status,
	},
};
#define N_CONTROLS (ARRAY_SIZE(sh833su_controls))

static struct sh833su_control *sh833su_find_control(u32 id)
{
	int i;

	for (i = 0; i < N_CONTROLS; i++)
		if (sh833su_controls[i].qc.id == id)
			return &sh833su_controls[i];

	return 0;
}

static int sh833su_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	struct sh833su_device *dev = to_sh833su_sensor(sd);
	struct sh833su_control *ctrl = sh833su_find_control(qc->id);

	sh833su_dbg("control ID: %u\n", qc->id);

	if (!ctrl)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	*qc = ctrl->qc;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static int sh833su_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct sh833su_device *dev = to_sh833su_sensor(sd);
	struct sh833su_control *s_ctrl;
	int ret;

	if (!ctrl)
		return -EINVAL;

	s_ctrl = sh833su_find_control(ctrl->id);
	if (!s_ctrl || !s_ctrl->query)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	ret = s_ctrl->query(sd, &ctrl->value);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int sh833su_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct sh833su_device *dev = to_sh833su_sensor(sd);
	struct sh833su_control *octrl = sh833su_find_control(ctrl->id);
	int ret;

	if (!octrl || !octrl->tweak)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	ret = octrl->tweak(sd, ctrl->value);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int sh833su_s_power(struct v4l2_subdev *sd, int on)
{
	struct sh833su_device *dev = to_sh833su_sensor(sd);
	int ret;

	if (dev->manual) {
		sh833su_info("manual focus control enabled; atomisp detached\n");
		return 0;
	}

	/* Switch the power */
	ret = sh833su_power_ctrl(dev, on);
	if (ret) {
		sh833su_err("cannot switch the power %s\n", on ? "on" : "off");
		return ret;
	}

	if (on) {
		/* Init HVCA */
		ret = sh833su_hvca_init(dev);
		if (ret) {
			sh833su_err("HVCA initialization failure: %d\n", ret);
			return ret;
		}

		/* Set initial lens position */
		ret = sh833su_hvca_set_initial_position(dev);
		if (ret) {
			sh833su_err("HVCA set initial position failure: %d\n",
					ret);
			return ret;
		}

	} else {
		dev->focus = 0;
		dev->inf = 0;
		dev->macro = 0;
		dev->hyper = 0;
	}

	return 0;
}

/* ---------------------------------------------------------------------- */
/* Atom ISP V4L2 core operations */

static const struct v4l2_subdev_core_ops sh833su_core_ops = {
	.queryctrl = sh833su_queryctrl,
	.g_ctrl = sh833su_g_ctrl,
	.s_ctrl = sh833su_s_ctrl,
	.s_power = sh833su_s_power,
};

static const struct v4l2_subdev_ops sh833su_ops = {
	.core = &sh833su_core_ops,
};


/* ---------------------------------------------------------------------- */
/* SH833-SU I2C functions */

static int sh833su_read_u8(struct i2c_client *client, u8 reg, u8 *val)
{
	int ret;

	struct i2c_msg msg[2];
	u8 buf[2];

	buf[0] = reg;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &buf[0];

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = &buf[1];

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret != 2)
		return ret;

	*val = buf[1];
	return 0;
}


static int sh833su_write_u8(struct i2c_client *client, u8 reg, u8 val)
{
	int ret;

	struct i2c_msg msg;
	u8 buf[2];

	buf[0] = reg;
	buf[1] = val;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = &buf[0];

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret != 1)
		return ret;

	return 0;
}

static int sh833su_read_u16(struct i2c_client *client, u8 reg, u16 *val)
{
	int ret;

	struct i2c_msg msg[2];
	u16 data;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &reg;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = (u8 *) &data;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret != 2)
		return ret;

	*val = be16_to_cpu(data);
	return 0;
}

static int sh833su_write_u16(struct i2c_client *client, u8 reg, u16 val)
{
	int ret;

	struct i2c_msg msg;
	u8 buf[3];

	buf[0] = reg;

	buf[1] = (u8) (val >> 8) & 0xff;
	buf[2] = (u8) (val & 0xff);

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 3;
	msg.buf = &buf[0];

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret != 1)
		return ret;

	return 0;
}


static int __sh833su_read_eeprom(struct i2c_client *client, u16 eeprom_addr,
		u8 start_reg, u8 *buffer, u16 length)
{
	int ret;

	struct i2c_msg msg[2];

	msg[0].addr = eeprom_addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &start_reg;

	msg[1].addr = eeprom_addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = length;
	msg[1].buf = &buffer[0];

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret != 2)
		return ret;

	return 0;
}

static int sh833su_read_eeprom(struct i2c_client *client, u8 *buffer)
{
	int attempt;
	int ret;

	for (attempt = 0; attempt < SH833SU_INIT_ATTEMPTS; ++attempt) {
		ret = __sh833su_read_eeprom(client, SH833SU_EEPROM_ADDRESS,
				0x0, buffer, SH833SU_EEPROM_BUFFER_SIZE);

		if (ret == -EAGAIN) {
			udelay(100);
			sh833su_err("eeprom read attempt #%d (-EAGAIN)\n",
					attempt + 1);
			continue;

		} else
			return ret;
	}

	return -ENODEV;
}

/* ---------------------------------------------------------------------- */
/* HVCA initialization functions */

static int sh833su_hvca_write_1byte(struct i2c_client *client,
		u8 *eeprom_buffer, u8 hvca_mode, u8 hvca_addr, u8 hvca_data1)
{
	u8 data = 0;
	int ret = 0;

	switch (hvca_mode) {
	case HVCA_DIRECT_MODE:
		sh833su_dbg(" -- mode: direct\n");
		data = hvca_data1;
		break;

	case HVCA_INDIRECT_EEPROM:
		sh833su_dbg(" -- mode: indirect EEPROM\n");
		data = eeprom_buffer[hvca_data1];
		break;

	case HVCA_INDIRECT_HVCA:
		sh833su_dbg(" -- mode: indirect HVCA\n");
		ret = sh833su_read_u8(client, hvca_data1, &data);
		break;

	case HVCA_MASK_AND:
		sh833su_dbg(" -- mode: mask AND\n");
		ret = sh833su_read_u8(client, hvca_addr, &data);
		data &= hvca_data1;
		break;

	case HVCA_MASK_OR:
		sh833su_dbg(" -- mode: mask OR\n");
		ret = sh833su_read_u8(client, hvca_addr, &data);
		data |= hvca_data1;
		break;

	default:
		sh833su_err(" ### unknown HVCA mode\n");
		ret = -EIO;
		break;
	}

	if (ret) {
		sh833su_err("unable to read input data: %d\n", ret);
		return ret;
	}

	return sh833su_write_u8(client, hvca_addr, data);
}

static int sh833su_hvca_write_2byte(struct i2c_client *client,
		u8 *eeprom_buffer, u8 hvca_mode, u8 hvca_addr, u8 hvca_data1,
		u8 hvca_data2)
{
	int ret;

	u8 data1 = 0;
	u8 data2 = 0;

	u16 data = 0;

	switch (hvca_mode) {
	case HVCA_DIRECT_MODE:
		sh833su_dbg(" -- mode: direct\n");

		data = (((u16)hvca_data1 << 8) & 0xFF00) |
			((u16)hvca_data2 & 0x00FF);
		break;

	case HVCA_INDIRECT_EEPROM:
		sh833su_dbg(" -- mode: indirect EEPROM\n");

		data1 = eeprom_buffer[hvca_data1];
		data2 = eeprom_buffer[hvca_data2];

		data = (((u16)data1 << 8) & 0xFF00) | ((u16)data2 & 0x00FF);
		break;

	case HVCA_INDIRECT_HVCA:
		sh833su_dbg(" -- mode: indirect HVCA\n");

		ret = sh833su_read_u8(client, hvca_data1, &data1);
		ret = ret | sh833su_read_u8(client, hvca_data2, &data2);

		data = (((u16)data1 << 8) & 0xFF00) | ((u16)data2 & 0x00FF);
		break;

	case HVCA_MASK_AND:
		sh833su_dbg(" -- mode: mask AND\n");

		ret = sh833su_read_u16(client, hvca_addr, &data);
		data = data & ((((u16)data1 << 8) & 0xFF00) |
				((u16)data2 & 0x00FF));
		break;

	case HVCA_MASK_OR:
		sh833su_dbg(" -- mode: mask OR\n");

		ret = sh833su_read_u16(client, hvca_addr, &data);
		data = data & ((((u16)data1 << 8) & 0xFF00) |
				((u16)data2 & 0x00FF));
		break;

	default:
		sh833su_err(" ### unknown HVCA mode\n");
		ret = -EIO;
		break;
	}

	if (ret) {
		sh833su_err("unable to read input data: %d\n", ret);
		return ret;
	}

	return sh833su_write_u16(client, hvca_addr, data);
}


static int sh833su_hvca_init(struct sh833su_device *dev)
{
	int ret;

	s16 inf_limit;
	s16 macro_limit;
	s16 inf_cal;
	s16 macro_cal;

	int eeprom_addr;

	struct i2c_client *client = dev->client;


	sh833su_memory_dump("Calibration data", &dev->eeprom[0x20], 0x8);

	inf_limit = sh833su_eeprom_calibration_data(dev->eeprom,
			SH833SU_EEPROM_MAP_INF1);
	macro_limit = sh833su_eeprom_calibration_data(dev->eeprom,
			SH833SU_EEPROM_MAP_MAC1);
	inf_cal = sh833su_eeprom_calibration_data(dev->eeprom,
			SH833SU_EEPROM_MAP_INF2);
	macro_cal = sh833su_eeprom_calibration_data(dev->eeprom,
			SH833SU_EEPROM_MAP_MAC2);

	/* Validate EEPROM positions */
	if (inf_limit <= macro_limit) {
		sh833su_err("hard limit positions invalid, using bogus "
				"limits\n");

		inf_limit = 2000;
		macro_limit = -2000;
	}

	if (inf_cal <= macro_cal) {
		sh833su_err("calibrated positions invalid, using hard "
				"limits\n");

		inf_cal = inf_limit;
		macro_cal = macro_limit;
	}

	/* Determine sweep limits */
	dev->inf = inf_cal + (((inf_cal - macro_cal)*412)/2244);
	dev->macro = macro_cal;

	/* Determine hyperfocal position */
	dev->hyper = inf_cal + (((inf_cal - macro_cal)*77)/2244);

	sh833su_dbg("inf limit: %d, macro limit: %d, "
		    "inf cal: %d, macro cal: %d, "
		    "inf: %d, macro: %d, hyper: %d\n",

			inf_limit, macro_limit, inf_cal, macro_cal,
			dev->inf, dev->macro, dev->hyper);

	/* Initialize driver */
	sh833su_dbg("HVCA initialization:\n");
	for (eeprom_addr = 0x30; eeprom_addr <= 0x1BF; eeprom_addr += 4) {
		u8 hvca_addr = dev->eeprom[eeprom_addr];
		u8 hvca_size = dev->eeprom[eeprom_addr + 1] & 0x0F;
		u8 hvca_mode = dev->eeprom[eeprom_addr + 1] & 0xF0;
		u8 hvca_data1 = dev->eeprom[eeprom_addr + 2];
		u8 hvca_data2 = dev->eeprom[eeprom_addr + 3];

		if (hvca_addr == 0xFF) {
			sh833su_dbg(" -- initialization data end\n");
			break;
		}

		if (hvca_addr == 0xDD) {
			u16 hvca_delay = (u16) (hvca_data1 << 8 | hvca_data2);
			sh833su_dbg(" -- delay: %u ms\n", hvca_delay);

			mdelay(hvca_delay);
			continue;
		}

		sh833su_dbg(" -- address: 0x%x\n", hvca_addr);
		sh833su_dbg(" -- size: 0x%x\n", hvca_size);
		sh833su_dbg(" -- data1: 0x%x, data2: 0x%x\n", hvca_data1,
				hvca_data2);

		if (hvca_size == HVCA_DATA_1BYTE) {
			ret = sh833su_hvca_write_1byte(client, dev->eeprom,
					hvca_mode, hvca_addr, hvca_data1);
			if (ret)
				goto init_error;

		} else if (hvca_size == HVCA_DATA_2BYTE) {
			ret = sh833su_hvca_write_2byte(client, dev->eeprom,
					hvca_mode, hvca_addr, hvca_data1,
					hvca_data2);
			if (ret)
				goto init_error;

		} else {
			sh833su_err("unknown HVCA data size\n");
			return -EIO;
		}
	}

	/* FIXME: Reduce to more feasible value */
	mdelay(50);

	return 0;

init_error:

	sh833su_err("HVCA initialization error: %d\n", ret);
	return ret;
}


/* ---------------------------------------------------------------------- */
/* HVCA control functions */

static int sh833su_move_driver(struct sh833su_device *dev, s16 target_pos)
{
	struct i2c_client *client = dev->client;

	s16 cur_pos, move_step;
	u16 move_dis;

	int ret;

	/* Read Current Position */
	ret = sh833su_read_u16(client, RZ_211H, &cur_pos);
	if (ret)
		goto move_driver_error;

	sh833su_dbg("RZ_211H: 0x%x (%d)\n", cur_pos, (s16) cur_pos);


	/* Check move distance to Target Position */
	move_dis = abs((int)cur_pos - (int)target_pos);

	/* If move distance is shorter than MS1Z12(=Step width) */
	if (move_dis <= STMV_SIZE) {
		ret = sh833su_write_u8(client, MSSET_211,
				(INI_MSSET_211 | 0x01));
		ret = ret | sh833su_write_u16(client, MS1Z22_211H, target_pos);
		if (ret)
			goto move_driver_error;
	} else {
		if (cur_pos < target_pos)
			move_step = STMV_SIZE;
		else
			move_step = -STMV_SIZE;

		/* Set StepMove Target Position */
		ret = sh833su_write_u16(client, MS1Z12_211H, move_step);
		ret |= sh833su_write_u16(client, STMVENDH_211, target_pos);

		/* Start StepMove */
		ret |= sh833su_write_u8(client, STMVEN_211, (STMCHTG_ON |
				STMSV_ON | STMLFF_OFF | STMVEN_ON));

#ifdef SH833SU_SANITY_CHECKS
		{
			u8 stmven_211_verify = 0xFF;

			u16 ms1z12_211h = 0xFF;
			u16 stmvendh_211 = 0xFF;


			ret = sh833su_read_u8(client, STMVEN_211,
					&stmven_211_verify);
			ret |= sh833su_read_u16(client, MS1Z12_211H,
					&ms1z12_211h);
			ret |= sh833su_read_u16(client, STMVENDH_211,
					&stmvendh_211);

			if (ret)
				goto move_driver_error;

			sh833su_dbg("STMVEN_211: 0x%x, MS1Z12_211H: 0x%x, "
					"STMVENDH_211: 0x%x\n",
					stmven_211_verify, ms1z12_211h,
					stmvendh_211);
		}
#endif

		if (ret)
			goto move_driver_error;
	}

	return 0;

move_driver_error:
	sh833su_err("focuser failed: %d\n", ret);
	return ret;
}

static int sh833su_wait_for_move(struct sh833su_device *dev)
{
	struct i2c_client *client = dev->client;

	u16 us_sm_fin;
	u8 uc_par_mod = 0xFF;

	u8 move_time = 0;
	u8 complete_time = 0;

	int ret;


	dev->status = SH833SU_STATUS_MOVING;

	do {
		mdelay(1);

		ret = sh833su_read_u8(client, STMVEN_211, &uc_par_mod);
		ret = ret | sh833su_read_u16(client, RZ_211H, &us_sm_fin);
		if (ret)
			goto wait_for_move_error;

		sh833su_dbg("STMVEN_211: 0x%x, RZ_211H: 0x%x\n",
				uc_par_mod, us_sm_fin);

		/* Step move error handling: unexpected position */
		if ((us_sm_fin == 0x7FFF) || (us_sm_fin == 0x8001)) {

			/* Terminate step move operation */
			ret = sh833su_write_u8(client, STMVEN_211,
					uc_par_mod & 0xFE);
			if (ret)
				goto wait_for_move_error;
		}
		move_time++;

		/* Wait StepMove operation end */
	} while ((uc_par_mod & STMVEN_ON) && (move_time < 50));

	if ((uc_par_mod & 0x08) == STMCHTG_ON) {
		u8 chg_test = 0xFF;

		mdelay(5);
		do {
			mdelay(1);

			complete_time++;
			ret = sh833su_read_u8(client, MSSET_211, &chg_test);
			if (ret)
				goto wait_for_move_error;

#ifdef SH833SU_SANITY_CHECKS
			{
				u16 rz_211h = 0xFF;
				ret = sh833su_read_u16(client, RZ_211H,
						&rz_211h);
				if (ret)
					goto wait_for_move_error;

				sh833su_dbg("MSSET_211: 0x%x, RZ_211H: 0x%x\n",
						chg_test, rz_211h);
			}
#endif

		} while ((chg_test & CHTGST_ON) && (complete_time < 15));
	}

	sh833su_dbg(" -- complete, move time: %d ms, finish time: %d\n",
			move_time, complete_time + 5);

	goto done;

wait_for_move_error:
	sh833su_err("focuser failed: %d\n", ret);


done:
	dev->status = SH833SU_STATUS_COMPLETE;
	return ret;
}

static int sh833su_lens_move_pulse(struct sh833su_device *dev, s16 target_pos)
{
	int ret;

	if (!dev->power) {
		sh833su_err("actuator is powered off\n");
		return -ENODEV;
	}

	ret = sh833su_move_driver(dev, target_pos);
	ret |= sh833su_wait_for_move(dev);

	return ret;
}

static int sh833su_hvca_set_initial_position(struct sh833su_device *dev)
{
	int ret;

	ret = sh833su_lens_move_pulse(dev, dev->inf);
	ret |= sh833su_lens_move_pulse(dev, dev->macro);
	ret |= sh833su_lens_move_pulse(dev, dev->hyper);

	if (ret)
		sh833su_err("HVCA initial position setting error: %d\n", ret);

	return ret;
}


/* ---------------------------------------------------------------------- */
/* HVCA power functions */

static int sh833su_power_ctrl(struct sh833su_device *dev, int on)
{
	int ret;

	ret = dev->platform_data->power_ctrl(&dev->sd, on);
	if (ret) {
		sh833su_err("power control function failed: %d\n", ret);
		return ret;
	}

	/* Platform init code should put delay after power on */
	dev->power = on ? 1 : 0;

	return 0;
}


/* ---------------------------------------------------------------------- */
/* Debug interface */

#ifdef CONFIG_DEBUG_FS

static ssize_t sh833su_debugfs_control_write(struct file *file,
		char const __user *buf,	size_t count, loff_t *offset)
{
	struct sh833su_device *dev = ((struct seq_file *)
			file->private_data)->private;

	int ret;

	char buffer[10];

	if (!dev->power) {
		sh833su_err("actuator is powered off\n");
		return -ENODEV;
	}

	if (copy_from_user(&buffer, buf, sizeof(buffer))) {
		sh833su_err("unable to read new focus position\n");
		return -EFAULT;
	}

	if (kstrtos16(buffer, 10, &dev->focus)) {
		sh833su_err("unable to convert 8 to 10 bit str\n");
		return -EFAULT;
	}

	if (dev->focus < dev->macro)
		dev->focus = dev->macro;

	else if (dev->focus > dev->inf)
		dev->focus = dev->inf;

	sh833su_dbg("Setting new focus position: %s (%d)", buffer, dev->focus);

	ret = sh833su_lens_move_pulse(dev, dev->focus);
	if (ret) {
		sh833su_err("Unable to set new lens position: %d\n", ret);
		return count;
	}

	sh833su_dbg("setting lens position was successful\n");

	return count;
}

static int sh833su_debugfs_control_show(struct seq_file *seq, void *unused)
{
	struct sh833su_device *dev = (struct sh833su_device *) seq->private;
	struct i2c_client *client = dev->client;

	u16 rz_211h = 0xFF;

	int ret;

	if (!dev->power) {
		seq_printf(seq, "actuator is powered off\n");
		return 0;
	}

	ret = sh833su_read_u16(client, RZ_211H, &rz_211h);
	if (ret) {
		sh833su_err("unable to read the Hall sensor (RZ_211H)\n");
		return ret;
	}

	sh833su_dbg("RZ_211H: 0x%x (%d)\n", rz_211h, (s16) rz_211h);

	seq_printf(seq, "%d\n", (s16) rz_211h);
	return 0;
}

static int sh833su_debugfs_control_open(struct inode *inode, struct file *file)
{
	return single_open(file, sh833su_debugfs_control_show,
			inode->i_private);
}

static ssize_t sh833su_debugfs_power_write(struct file *file,
		char const __user *buf, size_t count, loff_t *offset)
{
	struct sh833su_device *dev = ((struct seq_file *)
			file->private_data)->private;

	char buffer[2];
	u8 on;

	int ret;

	if (!dev->manual) {
		sh833su_err("unable to switch power manually in automatic mode\n");
		return -EACCES;
	}

	if (copy_from_user(&buffer, buf, sizeof(buffer))) {
		sh833su_err("unable to read new power setting\n");
		return -EFAULT;
	}

	if (kstrtou8(buffer, 10, &on)) {
		sh833su_err("unable to convert 8 to 10 bit str\n");
		return -EFAULT;
	}

	/* We are not using sh833su_s_power() function here since it
	 * doesn't work in manual mode to detach from atomisp */
	ret = sh833su_power_ctrl(dev, on);
	if (ret)
		return ret;

	if (on) {
		ret = sh833su_hvca_init(dev);
		ret |= sh833su_hvca_set_initial_position(dev);
		if (ret)
			return ret;
	}

	sh833su_dbg("power is %s\n", dev->power ? "on" : "off");
	return count;
}

static int sh833su_debugfs_power_show(struct seq_file *seq, void *unused)
{
	struct sh833su_device *dev = (struct sh833su_device *) seq->private;

	seq_printf(seq, "%d\n", dev->power);
	return 0;
}

static int sh833su_debugfs_power_open(struct inode *inode, struct file *file)
{
	return single_open(file, sh833su_debugfs_power_show, inode->i_private);
}

static int sh833su_debugfs_status_show(struct seq_file *seq, void *unused)
{
	struct sh833su_device *dev = (struct sh833su_device *) seq->private;

	if (dev->power)
		seq_printf(seq, "Power: on\n\n"
				"Inf:   %d\nMacro: %d\nHyper: %d\n\n"
				"Focus: %d\n",
				dev->inf, dev->macro, dev->hyper,
				dev->focus);
	else
		seq_printf(seq, "Power: off\n");

	return 0;
}

static int sh833su_debugfs_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, sh833su_debugfs_status_show, inode->i_private);
}


static const struct file_operations sh833su_debugfs_control_fops = {
	.open		= sh833su_debugfs_control_open,
	.read		= seq_read,
	.write		= sh833su_debugfs_control_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations sh833su_debugfs_power_fops = {
	.open		= sh833su_debugfs_power_open,
	.read		= seq_read,
	.write		= sh833su_debugfs_power_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations sh833su_debugfs_status_fops = {
	.open		= sh833su_debugfs_status_open,
	.read		= seq_read,
	.write		= 0,
	.llseek		= seq_lseek,
	.release	= single_release,
};


static void sh833su_create_debugfs(struct sh833su_device *dev)
{
	dev->sh833su_debugfs_dir = debugfs_create_dir("sh833su", 0);
	if (!dev->sh833su_debugfs_dir) {
		sh833su_err(" ### Cannot create debugfs folder\n");
		return;
	}

	debugfs_create_file("set_position",
			S_IWUGO | S_IRUGO,
			dev->sh833su_debugfs_dir, (void *) dev,
			&sh833su_debugfs_control_fops);

	debugfs_create_file("status",
			S_IRUGO,
			dev->sh833su_debugfs_dir, (void *) dev,
			&sh833su_debugfs_status_fops);

	debugfs_create_file("power",
			S_IWUGO | S_IRUGO,
			dev->sh833su_debugfs_dir, (void *) dev,
			&sh833su_debugfs_power_fops);

	debugfs_create_u8("manual", 0644, dev->sh833su_debugfs_dir,
			&dev->manual);
}
#else
	#define sh833su_create_debugfs(dev) do { /* do nothing */ } while (0)
#endif /* CONFIG_DEBUG_FS */


/* ---------------------------------------------------------------------- */
/* Initialization and probe */

static int sh833su_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct sh833su_device *dev;
	u8 sh833su_switch = 0xFF;

	int attempt;
	int ret;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -ENODEV;

	if (!client->dev.platform_data) {
		sh833su_err("platform data is not defined for sh833su\n");
		return -ENODEV;
	}

	/* Allocate actuator device */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		v4l2_err(client, "%s: out of memory\n", __func__);
		return -ENOMEM;
	}

	mutex_init(&dev->input_lock);

	dev->client = client;
	dev->platform_data = client->dev.platform_data;

	/* Create debugfs entries */
	sh833su_create_debugfs(dev);

	/* Enable power */
	sh833su_power_ctrl(dev, 1);


	/* Check SH833-SU I2C access */
	for (attempt = 0; attempt < SH833SU_INIT_ATTEMPTS; ++attempt) {
		ret = sh833su_read_u8(client, 0x80, &sh833su_switch);
		if (!ret)
			break;

		sh833su_info("attempt #%d: unable to read clock register: %d\n",
				attempt + 1, ret);

		udelay(10);
	}

	if (ret) {
		sh833su_err("sh833su read failed: %d\n", ret);
		return ret;
	}

	sh833su_info("AF control mode setting: 0x%x\n", sh833su_switch);

	/* Read EEPROM */
	ret = sh833su_read_eeprom(client, dev->eeprom);

	if (ret != 0) {
		sh833su_err("unable to read EEPROM\n");
		return ret;
	}

	/* TODO: EEPROM CRC check */
	/* */

	sh833su_memory_dump("EEPROM dump", dev->eeprom,
			SH833SU_EEPROM_BUFFER_SIZE);

	/* Register new motor/lens device */
	v4l2_i2c_subdev_init(&(dev->sd), client, &sh833su_ops);

	/* Turn off power */
	sh833su_power_ctrl(dev, 0);

	return 0;
}

static int sh833su_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sh833su_device *dev = to_sh833su_sensor(sd);

#ifdef CONFIG_DEBUG_FS
	debugfs_remove_recursive(dev->sh833su_debugfs_dir);
#endif

	v4l2_device_unregister_subdev(sd);
	kfree(dev);

	return 0;
}

static const struct i2c_device_id sh833su_id[] = {
	{SH833SU_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, sh833su_id);

static struct i2c_driver sh833su_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = SH833SU_NAME,
	},
	.probe = sh833su_probe,
	.remove = sh833su_remove,
	.id_table = sh833su_id,
};

static __init int init_sh833su(void)
{
	return i2c_add_driver(&sh833su_driver);
}

static __exit void exit_sh833su(void)
{
	i2c_del_driver(&sh833su_driver);
}

module_init(init_sh833su);
module_exit(exit_sh833su);

MODULE_DESCRIPTION("A low-level driver for SEMCO SH833-SU HVCA focuser");
MODULE_LICENSE("GPL");
