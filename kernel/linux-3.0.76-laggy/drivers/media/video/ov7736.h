/*
 * Support for ov7736 Camera Sensor.
 *
 * Copyright (c) 2010 Intel Corporation. All Rights Reserved.
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

#ifndef __OV7736_H__
#define __OV7736_H__

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <linux/spinlock.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <linux/v4l2-mediabus.h>
#include <media/media-entity.h>
#include <linux/atomisp_platform.h>
#include <linux/atomisp.h>

#define V4L2_IDENT_OV7736 7736

#define MT9P111_REV3
#define FULLINISUPPORT


/* #defines for register writes and register array processing */
#define MISENSOR_8BIT		1
#define MISENSOR_16BIT		2
#define MISENSOR_32BIT		4
#define MISENSOR_RMW		0x10

#define MISENSOR_FWBURST0	0x80
#define MISENSOR_FWBURST1	0x81
#define MISENSOR_FWBURST4	0x84
#define MISENSOR_FWBURST	0x88

#define MISENSOR_TOK_TERM	0xf000	/* terminating token for reg list */
#define MISENSOR_TOK_DELAY	0xfe00	/* delay token for reg list */
#define MISENSOR_TOK_FWLOAD	0xfd00	/* token indicating load FW */
#define MISENSOR_TOK_POLL	0xfc00	/* token indicating poll instruction */
#define MISENSOR_TOK_RMW	0x0010  /* RMW operation */
#define MISENSOR_TOK_MASK	0xfff0
#define MISENSOR_FLIP_EN	(1<<1)	/* enable vert_flip */
#define MISENSOR_MIRROR_EN	(1<<0)	/* enable horz_mirror */

/* mask to set sensor read_mode via misensor_rmw_reg */
#define MISENSOR_R_MODE_MASK	0x0330
/* mask to set sensor vert_flip and horz_mirror */
#define MISENSOR_F_M_MASK	0x0003

/* bits set to set sensor vert_flip and horz_mirror */
#define MISENSOR_F_M_EN	(MISENSOR_FLIP_EN | MISENSOR_MIRROR_EN)
#define MISENSOR_F_EN		MISENSOR_FLIP_EN
#define MISENSOR_F_M_DIS	(MISENSOR_FLIP_EN & MISENSOR_MIRROR_EN)

/* sensor register that control sensor read-mode and mirror */
#define MISENSOR_READ_MODE	0xC834

#define SENSOR_DETECTED		1
#define SENSOR_NOT_DETECTED	0

#define I2C_RETRY_COUNT		5
#define MSG_LEN_OFFSET		2
#define MAX_FMTS		1

#ifndef MIPI_CONTROL
#define MIPI_CONTROL		0x3400	/* MIPI_Control */
#endif

/* GPIO pin on Moorestown */
#define GPIO_SCLK_25		44
#define GPIO_STB_PIN		47

#define GPIO_STDBY_PIN		49   /* ab:new */
#define GPIO_RESET_PIN		50

/* System control register for Aptina A-1040SOC*/
#define OV7736_PID		0x0

/* MT9P111_DEVICE_ID */
#define OV7736_MOD_ID		0x7736

/* ulBPat; */

#define OV7736_BPAT_RGRGGBGB	(1 << 0)
#define OV7736_BPAT_GRGRBGBG	(1 << 1)
#define OV7736_BPAT_GBGBRGRG	(1 << 2)
#define OV7736_BPAT_BGBGGRGR	(1 << 3)

#define OV7736_FOCAL_LENGTH_NUM	1828	/* 1.828mm */
#define OV7736_FOCAL_LENGTH_DEN	1000
#define OV7736_F_NUMBER_NUM	28	/* 2.8 */
#define OV7736_F_NUMBER_DEN	10
#define OV7736_WAIT_STAT_TIMEOUT	100
#define OV7736_FLICKER_MODE_50HZ	1
#define OV7736_FLICKER_MODE_60HZ	2
/*
 * focal length bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define OV7736_FOCAL_LENGTH_DEFAULT 0x072403e8	/* 1828/1000 */

/*
 * current f-number bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define OV7736_F_NUMBER_DEFAULT 0x001c000a	/* 28/10 */

/*
 * f-number range bits definition:
 * bits 31-24: max f-number numerator
 * bits 23-16: max f-number denominator
 * bits 15-8: min f-number numerator
 * bits 7-0: min f-number denominator
 */
#define OV7736_F_NUMBER_RANGE 0x1c0a1c0a	/* 28/10,28/10 */

/* Supported resolutions */
enum {
	OV7736_RES_VGA,
};

#define OV7736_RES_VGA_SIZE_H		640
#define OV7736_RES_VGA_SIZE_V		480
/*
 * struct misensor_reg - MI sensor  register format
 * @length: length of the register
 * @reg: 16-bit offset to register
 * @val: 8/16/32-bit register value
 * Define a structure for sensor register initialization values
 */
struct misensor_reg {
	u32 length;
	u32 reg;
	u32 val;	/* value or for read/mod/write, AND mask */
	u32 val2;	/* optional; for rmw, OR mask */
};

/*
 * struct misensor_fwreg - Firmware burst command
 * @type: FW burst or 8/16 bit register
 * @addr: 16-bit offset to register or other values depending on type
 * @valx: data value for burst (or other commands)
 *
 * Define a structure for sensor register initialization values
 */
struct misensor_fwreg {
	u32	type;	/* type of value, register or FW burst string */
	u32	addr;	/* target address */
	u32	val0;
	u32	val1;
	u32	val2;
	u32	val3;
	u32	val4;
	u32	val5;
	u32	val6;
	u32	val7;
};

struct regval_list {
	u16 reg_num;
	u8 value;
};

struct ov7736_device {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_mbus_framefmt format;

	struct camera_sensor_platform_data *platform_data;
	int fmt_idx;
	int real_model_id;
	int nctx;
	int power;

	unsigned int bus_width;
	unsigned int mode;
	unsigned int field_inv;
	unsigned int field_sel;
	unsigned int ycseq;
	unsigned int conv422;
	unsigned int bpat;
	unsigned int hpol;
	unsigned int vpol;
	unsigned int edge;
	unsigned int bls;
	unsigned int gamma;
	unsigned int cconv;
	unsigned int res;
	unsigned int dwn_sz;
	unsigned int blc;
	unsigned int agc;
	unsigned int awb;
	unsigned int aec;
	/* extention SENSOR version 2 */
	unsigned int cie_profile;

	/* extention SENSOR version 3 */
	unsigned int flicker_freq;

	/* extension SENSOR version 4 */
	unsigned int smia_mode;
	unsigned int mipi_mode;

	/* Add name here to load shared library */
	unsigned int type;

	/*Number of MIPI lanes*/
	unsigned int mipi_lanes;
	char name[32];

	u8 lightfreq;
};

struct ov7736_format_struct {
	u8 *desc;
	u32 pixelformat;
	struct regval_list *regs;
};

struct ov7736_res_struct {
	u8 *desc;
	int res;
	int width;
	int height;
	int fps;
	bool used;
	struct regval_list *regs;
};

struct ov7736_control {
	struct v4l2_queryctrl qc;
	int (*query)(struct v4l2_subdev *sd, s32 *value);
	int (*tweak)(struct v4l2_subdev *sd, int value);
};

#define OV7736_MAX_WRITE_BUF_SIZE	128
struct ov7736_write_buffer {
	u16 addr;
	u8 data[OV7736_MAX_WRITE_BUF_SIZE];
};

struct ov7736_write_ctrl {
	int index;
	struct ov7736_write_buffer buffer;
};

/*
 * Modes supported by the ov7736 driver.
 * Please, keep them in ascending order.
 */
static struct ov7736_res_struct ov7736_res[] = {
	{
	  .desc	= "VGA",
	  .res	= OV7736_RES_VGA,
	  .width	= 640,
	  .height	= 480,
	  .fps	= 30,
	  .used	= 0,
	  .regs	= NULL,
	},
};
#define N_RES (ARRAY_SIZE(ov7736_res))

static const struct i2c_device_id ov7736_id[] = {
	{"ov7736", 0},
	{}
};

static struct misensor_reg const ov7736_suspend[] = {
	{MISENSOR_8BIT,  0x302e, 0x08},
	{MISENSOR_8BIT,  0x3623, 0x00},
	{MISENSOR_8BIT,  0x3614, 0x20},
	{MISENSOR_8BIT,  0x4805, 0xd0},
	{MISENSOR_8BIT,  0x300e, 0x14},
	{MISENSOR_8BIT,  0x3008, 0x42},
	{MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const ov7736_streaming[] = {
	{MISENSOR_8BIT,  0x302e, 0x00},
	{MISENSOR_8BIT,  0x3623, 0x03},
	{MISENSOR_8BIT,  0x3614, 0x00},
	{MISENSOR_8BIT,  0x4805, 0x10},
	{MISENSOR_8BIT,  0x300e, 0x04},
	{MISENSOR_8BIT,  0x3008, 0x02},
	{MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const ov7736_vga_init[] = {
	/*system setting*/
	{MISENSOR_8BIT,  0x3008, 0x82},
	{MISENSOR_TOK_DELAY, 0, 10},
	{MISENSOR_8BIT,  0x3008, 0x42},
	{MISENSOR_TOK_DELAY, 0, 10},
	{MISENSOR_8BIT,  0x3104, 0x03},
	{MISENSOR_8BIT,  0x3017, 0x7f},/*OE pads*/
	{MISENSOR_8BIT,  0x3018, 0xfc},/*OE pads*/
	/*analog settings*/
	{MISENSOR_8BIT,  0x3602, 0x14},
	{MISENSOR_8BIT,  0x3611, 0x44},
	{MISENSOR_8BIT,  0x3631, 0x22},
	{MISENSOR_8BIT,  0x3622, 0x00},
	{MISENSOR_8BIT,  0x3633, 0x25},
	{MISENSOR_8BIT,  0x370d, 0x04},
	{MISENSOR_8BIT,  0x3620, 0x32},
	{MISENSOR_8BIT,  0x3714, 0x2c},
	{MISENSOR_8BIT,  0x401c, 0x00},
	/*digital settings*/
	{MISENSOR_8BIT,  0x401e, 0x11},
	{MISENSOR_8BIT,  0x4702, 0x01},
	{MISENSOR_8BIT,  0x5000, 0x0e},
	{MISENSOR_8BIT,  0x5001, 0x01},
	{MISENSOR_8BIT,  0x3a00, 0x7a},
	{MISENSOR_8BIT,  0x3a18, 0x00},
	{MISENSOR_8BIT,  0x3a19, 0x3f},
	/*format control*/
	{MISENSOR_8BIT,  0x300f, 0x88},
	{MISENSOR_8BIT,  0x3011, 0x08},

	{MISENSOR_8BIT,  0x4303, 0xff},
	{MISENSOR_8BIT,  0x4307, 0xff},
	{MISENSOR_8BIT,  0x430b, 0xff},
	{MISENSOR_8BIT,  0x4305, 0x00},
	{MISENSOR_8BIT,  0x4309, 0x00},
	{MISENSOR_8BIT,  0x430d, 0x00},
	/*isp control, enable/disable functions*/
	{MISENSOR_8BIT,  0x5000, 0x4f},
	{MISENSOR_8BIT,  0x5001, 0x47},
	/*format control*/
	{MISENSOR_8BIT,  0x4300, 0x30}, /*yuv422*/
	{MISENSOR_8BIT,  0x4301, 0x80}, /* ?*/
	/*isp top control*/
	{MISENSOR_8BIT,  0x501f, 0x01}, /*isp format yuv422*/
	/*mipi config*/
	{MISENSOR_8BIT,  0x3017, 0x00},
	{MISENSOR_8BIT,  0x3018, 0x00},
	{MISENSOR_8BIT,  0x300e, 0x04},
	{MISENSOR_8BIT,  0x4801, 0x0f},
	{MISENSOR_8BIT,  0x4601, 0x02},
	/*pll config*/
	{MISENSOR_8BIT,  0x300f, 0x8a},
	{MISENSOR_8BIT,  0x3012, 0x43},
	{MISENSOR_8BIT,  0x3011, 0x07},
	{MISENSOR_8BIT,  0x3010, 0x00},
	/*output format*/
	{MISENSOR_8BIT,  0x4300, 0x3f},/*yuv422, pixel order changed*/
	/* AWB control*/
	{MISENSOR_8BIT,  0x5180, 0x02},
	{MISENSOR_8BIT,  0x5181, 0x02},
	/* AEC/AGC control */
	{MISENSOR_8BIT,  0x3a0f, 0x35},
	{MISENSOR_8BIT,  0x3a10, 0x2c},
	{MISENSOR_8BIT,  0x3a1b, 0x36},
	{MISENSOR_8BIT,  0x3a1e, 0x2d},
	{MISENSOR_8BIT,  0x3a11, 0x90},
	{MISENSOR_8BIT,  0x3a1f, 0x10},
	/*isp general control, enable/disable functions*/
	{MISENSOR_8BIT,  0x5000, 0xcf},
	/*gamma curve*/
	{MISENSOR_8BIT,  0x5481, 0x08},
	{MISENSOR_8BIT,  0x5482, 0x18},
	{MISENSOR_8BIT,  0x5483, 0x38},
	{MISENSOR_8BIT,  0x5484, 0x60},
	{MISENSOR_8BIT,  0x5485, 0x6e},
	{MISENSOR_8BIT,  0x5486, 0x78},
	{MISENSOR_8BIT,  0x5487, 0x81},
	{MISENSOR_8BIT,  0x5488, 0x89},
	{MISENSOR_8BIT,  0x5489, 0x90},
	{MISENSOR_8BIT,  0x548a, 0x97},
	{MISENSOR_8BIT,  0x548b, 0xa5},
	{MISENSOR_8BIT,  0x548c, 0xb0},
	{MISENSOR_8BIT,  0x548d, 0xc5},
	{MISENSOR_8BIT,  0x548e, 0xd8},
	{MISENSOR_8BIT,  0x548f, 0xe9},
	{MISENSOR_8BIT,  0x5490, 0x1f},
	/*YUVmatrix*/
	{MISENSOR_8BIT,  0x5380, 0x50},
	{MISENSOR_8BIT,  0x5381, 0x43},
	{MISENSOR_8BIT,  0x5382, 0x0D},
	{MISENSOR_8BIT,  0x5383, 0x10},
	{MISENSOR_8BIT,  0x5384, 0x3f},
	{MISENSOR_8BIT,  0x5385, 0x50},
	{MISENSOR_8BIT,  0x5392, 0x1e},
	/*LSC*/
	{MISENSOR_8BIT,  0x5801, 0x00},
	{MISENSOR_8BIT,  0x5802, 0x00},
	{MISENSOR_8BIT,  0x5803, 0x00},
	{MISENSOR_8BIT,  0x5804, 0x34},
	{MISENSOR_8BIT,  0x5805, 0x28},
	{MISENSOR_8BIT,  0x5806, 0x22},
	/*uv avarage*/
	{MISENSOR_8BIT,  0x5001, 0xc7},
	/*demosaic ctrl*/
	{MISENSOR_8BIT,  0x5580, 0x02},
	{MISENSOR_8BIT,  0x5583, 0x40},
	{MISENSOR_8BIT,  0x5584, 0x26},
	{MISENSOR_8BIT,  0x5589, 0x10},
	{MISENSOR_8BIT,  0x558a, 0x00},
	{MISENSOR_8BIT,  0x558b, 0x3e},
	{MISENSOR_8BIT,  0x5300, 0x0f},
	{MISENSOR_8BIT,  0x5301, 0x30},
	{MISENSOR_8BIT,  0x5302, 0x0d},
	{MISENSOR_8BIT,  0x5303, 0x02},
	{MISENSOR_8BIT,  0x5300, 0x04},
	{MISENSOR_8BIT,  0x5301, 0x20},
	{MISENSOR_8BIT,  0x5302, 0x0a},
	{MISENSOR_8BIT,  0x5303, 0x00},
	{MISENSOR_8BIT,  0x5304, 0x10},
	{MISENSOR_8BIT,  0x5305, 0xCE},
	{MISENSOR_8BIT,  0x5306, 0x05},
	{MISENSOR_8BIT,  0x5307, 0xd0},
	/* AE statistics control */
	{MISENSOR_8BIT,  0x5680, 0x00},
	{MISENSOR_8BIT,  0x5681, 0x50},
	{MISENSOR_8BIT,  0x5682, 0x00},
	{MISENSOR_8BIT,  0x5683, 0x3c},
	{MISENSOR_8BIT,  0x5684, 0x11},
	{MISENSOR_8BIT,  0x5685, 0xe0},
	{MISENSOR_8BIT,  0x5686, 0x0d},
	{MISENSOR_8BIT,  0x5687, 0x68},
	{MISENSOR_8BIT,  0x5688, 0x03},
	/*timing control*/
	{MISENSOR_16BIT,  0x380c, 0x0314},/*HTS  788*/
	{MISENSOR_16BIT,  0x380e, 0x0238},/*VTS  568*/
	/*H/V mirror/flip control*/
	{MISENSOR_8BIT|MISENSOR_RMW, 0x3818, 0x60, 0x03},

	{MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const ov7736_antiflicker_50hz[] = {
	{MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const ov7736_antiflicker_60hz[] = {
	{MISENSOR_TOK_TERM, 0, 0}
};

#endif
