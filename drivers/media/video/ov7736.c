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

#define DEBUG

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
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>

#include "ov7736.h"

/* Uncomment to enable waiting for 3A algorithm to finish */
/* #define OV7736_WAIT_3A */

#define to_ov7736_sensor(sd) container_of(sd, struct ov7736_device, sd)

/*
 * TODO: use debug parameter to actually define when debug messages should
 * be printed.
 */
static int debug;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Debug level (0-1)");

/* FIXME: remove these hacks after sensor programming is fixed */
static int ov7736_factory_targetexp = -1;
static int ov7736_factory_unitygamma;
static int ov7736_factory_disableee;
static int ov7736_factory_disablelsc;

static int ov7736_t_vflip(struct v4l2_subdev *sd, int value);
static int ov7736_t_hflip(struct v4l2_subdev *sd, int value);
static int
ov7736_read_reg(struct i2c_client *client, u16 data_length, u32 reg, u32 *val)
{
	int err;
	struct i2c_msg msg[2];
	unsigned char data[4];

	if (!client->adapter) {
		v4l2_err(client, "%s error, no client->adapter\n", __func__);
		return -ENODEV;
	}

	if (data_length != MISENSOR_8BIT && data_length != MISENSOR_16BIT
					 && data_length != MISENSOR_32BIT) {
		v4l2_err(client, "%s error, invalid data length\n", __func__);
		return -EINVAL;
	}

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = MSG_LEN_OFFSET;
	msg[0].buf = data;

	/* high byte goes out first */
	data[0] = (u16) (reg >> 8);
	data[1] = (u16) (reg & 0xff);

	msg[1].addr = client->addr;
	msg[1].len = data_length;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = data;

	err = i2c_transfer(client->adapter, msg, 2);

	if (err >= 0) {
		*val = 0;
		/* high byte comes first */
		if (data_length == MISENSOR_8BIT)
			*val = data[0];
		else if (data_length == MISENSOR_16BIT)
			*val = data[1] + (data[0] << 8);
		else
			*val = data[3] + (data[2] << 8) +
			    (data[1] << 16) + (data[0] << 24);

		return 0;
	}

	dev_err(&client->dev, "read from offset 0x%x error %d", reg, err);
	return err;
}

static int
ov7736_write_reg(struct i2c_client *client, u16 data_length, u16 reg, u32 val)
{
	int num_msg;
	struct i2c_msg msg;
	unsigned char data[6] = {0};
	u16 *wreg;
	int retry = 0;

	if (!client->adapter) {
		v4l2_err(client, "%s error, no client->adapter\n", __func__);
		return -ENODEV;
	}

	if (data_length != MISENSOR_8BIT && data_length != MISENSOR_16BIT
					 && data_length != MISENSOR_32BIT) {
		v4l2_err(client, "%s error, invalid data_length\n", __func__);
		return -EINVAL;
	}

	memset(&msg, 0, sizeof(msg));

again:
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 2 + data_length;
	msg.buf = data;

	/* high byte goes out first */
	wreg = (u16 *)data;
	*wreg = cpu_to_be16(reg);

	if (data_length == MISENSOR_8BIT) {
		data[2] = (u8)(val);
	} else if (data_length == MISENSOR_16BIT) {
		u16 *wdata = (u16 *)&data[2];
		*wdata = be16_to_cpu((u16)val);
	} else {
		/* MISENSOR_32BIT */
		u32 *wdata = (u32 *)&data[2];
		*wdata = be32_to_cpu(val);
	}

	num_msg = i2c_transfer(client->adapter, &msg, 1);

	/*
	 * HACK: Need some delay here for Rev 2 sensors otherwise some
	 * registers do not seem to load correctly.
	 */
	mdelay(1);

	if (num_msg >= 0)
		return 0;

	dev_err(&client->dev, "write error: wrote 0x%x to offset 0x%x error %d",
		val, reg, num_msg);
	if (retry <= I2C_RETRY_COUNT) {
		dev_err(&client->dev, "retrying... %d", retry);
		retry++;
		msleep(20);
		goto again;
	}

	return num_msg;
}

/**
 * misensor_rmw_reg - Read/Modify/Write a value to a register in the sensor
 * device
 * @client: i2c driver client structure
 * @data_length: 8/16/32-bits length
 * @reg: register address
 * @mask: masked out bits
 * @set: bits set
 *
 * Read/modify/write a value to a register in the  sensor device.
 * Returns zero if successful, or non-zero otherwise.
 */
int misensor_rmw_reg(struct i2c_client *client, u16 data_length, u16 reg,
		     u32 mask, u32 set)
{
	int err;
	u32 val;

	/* Exit when no mask */
	if (mask == 0)
		return 0;

	/* @mask must not exceed data length */
	switch (data_length) {
	case MISENSOR_8BIT:
		if (mask & ~0xff)
			return -EINVAL;
		break;
	case MISENSOR_16BIT:
		if (mask & ~0xffff)
			return -EINVAL;
		break;
	case MISENSOR_32BIT:
		break;
	default:
		/* Wrong @data_length */
		return -EINVAL;
	}

	err = ov7736_read_reg(client, data_length, reg, &val);
	if (err) {
		v4l2_err(client, "misensor_rmw_reg error exit, read failed\n");
		return -EINVAL;
	}

	val &= ~mask;

	/*
	 * Perform the OR function if the @set exists.
	 * Shift @set value to target bit location. @set should set only
	 * bits included in @mask.
	 *
	 * REVISIT: This function expects @set to be non-shifted. Its shift
	 * value is then defined to be equal to mask's LSB position.
	 * How about to inform values in their right offset position and avoid
	 * this unneeded shift operation?
	 */
	set <<= ffs(mask) - 1;
	val |= set & mask;

	err = ov7736_write_reg(client, data_length, reg, val);
	if (err) {
		v4l2_err(client, "misensor_rmw_reg error exit, write failed\n");
		return -EINVAL;
	}

	return 0;
}


/*
 * ov7736_write_reg_array - Initializes a list of OV7736 registers
 * @client: i2c driver client structure
 * @reglist: list of registers to be written
 *
*/

int ov7736_write_reg_array(struct i2c_client *client,
			    const struct misensor_reg *reglist)
{
	const struct misensor_reg *next = reglist;
	int err;

	for (; next->length != MISENSOR_TOK_TERM; next++) {
		if (next->length == MISENSOR_TOK_DELAY) {
			msleep(next->val);
		} else if (next->length & MISENSOR_RMW) {
			err = misensor_rmw_reg(client,
					       next->length & ~MISENSOR_RMW,
					       next->reg, next->val,
					       next->val2);
			if (err) {
				dev_err(&client->dev, "%s read err. aborted\n",
					__func__);
				return err;
			}
		} else {
			err = ov7736_write_reg(client, next->length, next->reg,
						next->val);
			/* REVISIT: Do we need this delay? */
			udelay(10);
			if (err) {
				dev_err(&client->dev, "%s err. aborted\n",
					__func__);
				return err;
			}
		}
	}

	return 0;
}

#ifdef OV7736_WAIT_3A
static int ov7736_wait_3a(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	static int stabilize_timeout = 700;

#if 1
	v4l2_info(client, "3a stabilize stub timeout: %d\n",
		stabilize_timeout);

	msleep(stabilize_timeout);
	return 0;
#else
	int timeout = 100;
	int status;

	while (timeout--) {
		ov7736_read_reg(client, MISENSOR_16BIT, 0xA800, &status);
		if (status & 0x8) {
			v4l2_info(client, "3a stablize time:%dms.\n",
				  (100-timeout)*20);
			return 0;
		}
		msleep(20);
	}

	return -EINVAL;
#endif
}
#endif

static int ov7736_wait_state(struct v4l2_subdev *sd, int timeout)
{
#if 1
	return 0;
#else
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int val, ret;

	while (timeout-- > 0) {
		ret = ov7736_read_reg(client, MISENSOR_16BIT, 0x0080, &val);
		if (ret)
			return ret;
		if ((val & 0x2) == 0)
			return 0;
		msleep(20);
	}

	return -EINVAL;
#endif
}

static int ov7736_set_suspend(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	pr_debug("%s\n", __func__);

	ret = ov7736_write_reg_array(client, ov7736_suspend);
	if (ret)
		dev_err(&client->dev, "%s failed: %d\n", __func__, ret);

	/*msleep(100);*/

	/* ret = ov7736_wait_state(sd, 100); */

	return ret;
}

static int ov7736_set_streaming(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	pr_debug("%s\n", __func__);

	ret = ov7736_write_reg_array(client, ov7736_streaming);
	if (ret)
		dev_err(&client->dev, "%s failed: %d\n", __func__, ret);

	msleep(100);

	return ret;
}

static int ov7736_init_common(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = ov7736_write_reg_array(client, ov7736_vga_init);
	if (ret)
		dev_err(&client->dev, "%s failed: %d\n", __func__, ret);

	msleep(100);

	return ret;
}

static int power_up(struct v4l2_subdev *sd)
{
	struct ov7736_device *dev = to_ov7736_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	pr_debug("%s\n", __func__);

	if (NULL == dev->platform_data) {
		dev_err(&client->dev, "no camera_sensor_platform_data");
		return -ENODEV;
	}

	/* power control */
	ret = dev->platform_data->power_ctrl(sd, 1);
	if (ret)
		goto fail_power;

	/* flis clock control */
	ret = dev->platform_data->flisclk_ctrl(sd, 1);
	if (ret)
		goto fail_clk;

	/* gpio ctrl */
	ret = dev->platform_data->gpio_ctrl(sd, 1);
	if (ret)
		dev_err(&client->dev, "gpio failed 1\n");
	/*msleep(200);*/

	return 0;

fail_clk:
	dev->platform_data->flisclk_ctrl(sd, 0);
fail_power:
	dev->platform_data->power_ctrl(sd, 0);
	dev_err(&client->dev, "sensor power-up failed\n");

	return ret;
}

static int power_down(struct v4l2_subdev *sd)
{
	struct ov7736_device *dev = to_ov7736_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	pr_debug("%s\n", __func__);

	if (NULL == dev->platform_data) {
		dev_err(&client->dev, "no camera_sensor_platform_data");
		return -ENODEV;
	}

	ret = dev->platform_data->flisclk_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "flisclk failed\n");

	/* gpio ctrl */
	ret = dev->platform_data->gpio_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "gpio failed 1\n");

	/* power control */
	ret = dev->platform_data->power_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "vprog failed.\n");

	return ret;
}

static int ov7736_s_power(struct v4l2_subdev *sd, int power)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	pr_debug("%s %d\n", __func__, power);

	if (power == 0)
		return power_down(sd);
	else {
		if (power_up(sd))
			return -EINVAL;
		return 0;
		/*return ov7736_init_common(sd);*/
	}
}

static int ov7736_get_module_info(struct v4l2_subdev *sd,
		struct atomisp_factory_module_info *modinfo)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int ov7736_set_target_exp(struct v4l2_subdev *sd, int *exp)
{
	int ret;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u32 low, high;

	pr_debug("%s %d\n", __func__, *exp);

	if (*exp < -1 || *exp > 255) {
		v4l2_err(client, "value out of range\n");
		return -EINVAL;
	}

	if (*exp == -1) {
		low = 0x68;
		high = 0x78;
	} else {
		if (*exp <= 4) {
			high = 4;
			low = 0;
		} else {
			high = *exp;
			low = high - 4;
		}
	}

	ret = ov7736_write_reg(client, 1, 0x3A0F, high);
	if (ret)
		return ret;

	ret = ov7736_write_reg(client, 1, 0x3A10, low);
	if (ret)
		return ret;

	ret = ov7736_write_reg(client, 1, 0x3A1E, high);
	if (ret)
		return ret;

	ret = ov7736_write_reg(client, 1, 0x3A1B, low);
	if (ret)
		return ret;

	ov7736_factory_targetexp = *exp;

	return 0;
}

static int ov7736_set_unity_gamma(struct v4l2_subdev *sd, int *flag)
{
	int ret;
	u32 val;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	pr_debug("%s %d\n", __func__, *flag);

	ret = ov7736_read_reg(client, 1, 0x5000, &val);
	if (ret)
		return ret;

	if (*flag)
		val &= 0xbf; /* unity gamma */
	else
		val |= 0x40; /* normal gamma */

	ret = ov7736_write_reg(client, 1, 0x5000, val);
	if (ret)
		return ret;

	ov7736_factory_unitygamma = *flag;

	return 0;
}

static int ov7736_set_disable_ee(struct v4l2_subdev *sd, int *flag)
{
	static u32 saved_5300;
	static u32 saved_5301;

	int ret;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	pr_debug("%s %d\n", __func__, *flag);

	if (*flag) {
		/* disable ee */
		if (saved_5300 == 0) {
			ret = ov7736_read_reg(client, 1, 0x5300, &saved_5300);
			if (ret)
				return ret;
		}

		if (saved_5301 == 0) {
			ret = ov7736_read_reg(client, 1, 0x5301, &saved_5301);
			if (ret)
				return ret;
		}

		ret = ov7736_write_reg(client, 1, 0x5300, 0x00);
		if (ret)
			return ret;

		ret = ov7736_write_reg(client, 1, 0x5301, 0x00);
		if (ret)
			return ret;
	} else {
		/* enable ee */
		if (saved_5300 == 0 || saved_5301 == 0) {
			v4l2_err(client, "saved values invalid (%d/%d)\n",
					saved_5300, saved_5301);
			return -EINVAL;
		}

		ret = ov7736_write_reg(client, 1, 0x5300, saved_5300);
		if (ret)
			return ret;

		ret = ov7736_write_reg(client, 1, 0x5301, saved_5301);
		if (ret)
			return ret;

		saved_5300 = saved_5301 = 0;
	}

	ov7736_factory_disableee = *flag;

	return 0;
}

static int ov7736_set_disable_lsc(struct v4l2_subdev *sd, int *flag)
{
	int ret;
	u32 val;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	pr_debug("%s %d\n", __func__, *flag);

	ret = ov7736_read_reg(client, 1, 0x5000, &val);
	if (ret)
		return ret;

	if (*flag)
		val &= 0x7f; /* disable LSC */
	else
		val |= 0x80; /* enable LSC */

	ret = ov7736_write_reg(client, 1, 0x5000, val);
	if (ret)
		return ret;

	ov7736_factory_disablelsc = *flag;

	return 0;
}

static int ov7736_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	switch (cmd) {
	case ATOMISP_IOC_G_FACTORY_MODULE_INFO:
		return ov7736_get_module_info(sd,
				(struct atomisp_factory_module_info *)arg);
	case ATOMISP_IOC_S_FACTORY_TARGET_EXPOSURE:
		return ov7736_set_target_exp(sd, (int *)arg);
	case ATOMISP_IOC_S_FACTORY_UNITY_GAMMA:
		return ov7736_set_unity_gamma(sd, (int *)arg);
	case ATOMISP_IOC_S_FACTORY_DISABLE_EE:
		return ov7736_set_disable_ee(sd, (int *)arg);
	case ATOMISP_IOC_S_FACTORY_DISABLE_LSC:
		return ov7736_set_disable_lsc(sd, (int *)arg);
	default:
		pr_err("ov7736: unsupported cmd %d\n", cmd);
		return -EINVAL;
	}
	return -EINVAL;
}

static int ov7736_try_res(u32 *w, u32 *h)
{
	int i;

	/*
	 * The mode list is in ascending order. We're done as soon as
	 * we have found the first equal or bigger size.
	 */
	for (i = 0; i < N_RES; i++) {
		if ((ov7736_res[i].width >= *w) &&
		    (ov7736_res[i].height >= *h))
			break;
	}

	/*
	 * If no mode was found, it means we can provide only a smaller size.
	 * Returning the biggest one available in this case.
	 */
	if (i == N_RES)
		i--;

	*w = ov7736_res[i].width;
	*h = ov7736_res[i].height;

	return 0;
}

static struct ov7736_res_struct *ov7736_to_res(u32 w, u32 h)
{
	int  index;

	for (index = 0; index < N_RES; index++) {
		if ((ov7736_res[index].width == w) &&
		    (ov7736_res[index].height == h))
			break;
	}

	/* No mode found */
	if (index >= N_RES)
		return NULL;

	return &ov7736_res[index];
}

static int ov7736_try_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	return ov7736_try_res(&fmt->width, &fmt->height);
}

static int ov7736_res2size(unsigned int res, int *h_size, int *v_size)
{
	if (res != OV7736_RES_VGA) {
		WARN(1, "%s: Resolution 0x%08x unknown\n", __func__, res);
		return -EINVAL;
	}

	*h_size = OV7736_RES_VGA_SIZE_H;
	*v_size = OV7736_RES_VGA_SIZE_V;

	return 0;
}

static int ov7736_get_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	struct ov7736_device *dev = to_ov7736_sensor(sd);
	int width, height;
	int ret;

	ret = ov7736_res2size(dev->res, &width, &height);
	if (ret)
		return ret;
	fmt->width = width;
	fmt->height = height;

	return 0;
}

static int ov7736_set_mbus_fmt(struct v4l2_subdev *sd,
			      struct v4l2_mbus_framefmt *fmt)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	struct ov7736_device *dev = to_ov7736_sensor(sd);
	struct ov7736_res_struct *res_index;
	u32 width = fmt->width;
	u32 height = fmt->height;
	int ret;

	pr_debug("%s %dx%d\n", __func__, width, height);

	ov7736_try_res(&width, &height);
	res_index = ov7736_to_res(width, height);

	/* Sanity check */
	if (unlikely(!res_index)) {
		WARN_ON(1);
		return -EINVAL;
	}

	ret = ov7736_write_reg_array(c, ov7736_vga_init);
	if (ret)
		return -EINVAL;


	if (dev->res != res_index->res) {
		int index;

		/* Switch to different size */
		if (width <= 640) {
			dev->nctx = 0x00; /* Set for context A */
		} else {
			/*
			 * Context B is used for resolutions larger than 640x480
			 * Using YUV for Context B.
			 */
			dev->nctx = 0x01; /* set for context B */
		}

		/*
		 * Marked current sensor res as being "used"
		 *
		 * REVISIT: We don't need to use an "used" field on each mode
		 * list entry to know which mode is selected. If this
		 * information is really necessary, how about to use a single
		 * variable on sensor dev struct?
		 */
		for (index = 0; index < N_RES; index++) {
			if ((width == ov7736_res[index].width) &&
			    (height == ov7736_res[index].height)) {
				ov7736_res[index].used = 1;
				continue;
			}
			ov7736_res[index].used = 0;
		}
	}

	/*
	 * ov7736 - we don't poll for context switch
	 * because it does not happen with streaming disabled.
	 */
	dev->res = res_index->res;

	fmt->width = width;
	fmt->height = height;

	/* FIXME: re-apply factory settings */
	if (ov7736_factory_targetexp >= 0)
		ov7736_set_target_exp(sd, &ov7736_factory_targetexp);
	if (ov7736_factory_unitygamma)
		ov7736_set_unity_gamma(sd, &ov7736_factory_unitygamma);
	if (ov7736_factory_disableee)
		ov7736_set_disable_ee(sd, &ov7736_factory_disableee);
	if (ov7736_factory_disablelsc)
		ov7736_set_disable_lsc(sd, &ov7736_factory_disablelsc);

	return 0;
}

/* TODO: Update to SOC functions, remove exposure and gain */
static int ov7736_g_focal(struct v4l2_subdev *sd, s32 * val)
{
	*val = (OV7736_FOCAL_LENGTH_NUM << 16) | OV7736_FOCAL_LENGTH_DEN;
	return 0;
}

static int ov7736_g_fnumber(struct v4l2_subdev *sd, s32 * val)
{
	/*const f number for ov7736*/
	*val = (OV7736_F_NUMBER_NUM << 16) | OV7736_F_NUMBER_DEN;
	return 0;
}

static int ov7736_g_fnumber_range(struct v4l2_subdev *sd, s32 * val)
{
	*val = (OV7736_F_NUMBER_NUM << 24) |
		(OV7736_F_NUMBER_DEN << 16) |
		(OV7736_F_NUMBER_NUM << 8) | OV7736_F_NUMBER_DEN;
	return 0;
}

static int ov7736_s_freq(struct v4l2_subdev *sd, s32  val)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	struct ov7736_device *dev = to_ov7736_sensor(sd);
	int ret;

	pr_debug("%s %d\n", __func__, val);

	if (val != OV7736_FLICKER_MODE_50HZ &&
			val != OV7736_FLICKER_MODE_60HZ)
		return -EINVAL;

	if (val == OV7736_FLICKER_MODE_50HZ) {
		ret = ov7736_write_reg_array(c, ov7736_antiflicker_50hz);
		if (ret < 0)
			return ret;
	} else {
		ret = ov7736_write_reg_array(c, ov7736_antiflicker_60hz);
		if (ret < 0)
			return ret;
	}

	ret = ov7736_wait_state(sd, OV7736_WAIT_STAT_TIMEOUT);
	if (ret == 0)
		dev->lightfreq = val;

	return ret;
}

static struct ov7736_control ov7736_controls[] = {
	{
		.qc = {
			.id = V4L2_CID_VFLIP,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "Image v-Flip",
			.minimum = 0,
			.maximum = 1,
			.step = 1,
			.default_value = 0,
		},
		.tweak = ov7736_t_vflip,
	},
	{
		.qc = {
			.id = V4L2_CID_HFLIP,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "Image h-Flip",
			.minimum = 0,
			.maximum = 1,
			.step = 1,
			.default_value = 0,
		},
		.tweak = ov7736_t_hflip,
	},
	{
		.qc = {
			.id = V4L2_CID_FOCAL_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focal length",
			.minimum = OV7736_FOCAL_LENGTH_DEFAULT,
			.maximum = OV7736_FOCAL_LENGTH_DEFAULT,
			.step = 0x01,
			.default_value = OV7736_FOCAL_LENGTH_DEFAULT,
			.flags = 0,
		},
		.query = ov7736_g_focal,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number",
			.minimum = OV7736_F_NUMBER_DEFAULT,
			.maximum = OV7736_F_NUMBER_DEFAULT,
			.step = 0x01,
			.default_value = OV7736_F_NUMBER_DEFAULT,
			.flags = 0,
		},
		.query = ov7736_g_fnumber,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_RANGE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number range",
			.minimum = OV7736_F_NUMBER_RANGE,
			.maximum =  OV7736_F_NUMBER_RANGE,
			.step = 0x01,
			.default_value = OV7736_F_NUMBER_RANGE,
			.flags = 0,
		},
		.query = ov7736_g_fnumber_range,
	},
	{
		.qc = {
			.id = V4L2_CID_POWER_LINE_FREQUENCY,
			.type = V4L2_CTRL_TYPE_MENU,
			.name = "Light frequency filter",
			.minimum = 1,
			.maximum =  2, /* 1: 50Hz, 2:60Hz */
			.step = 1,
			.default_value = 1,
			.flags = 0,
		},
		.tweak = ov7736_s_freq,
	},

};
#define N_CONTROLS (ARRAY_SIZE(ov7736_controls))

static struct ov7736_control *ov7736_find_control(__u32 id)
{
	int i;

	for (i = 0; i < N_CONTROLS; i++) {
		if (ov7736_controls[i].qc.id == id)
			return &ov7736_controls[i];
	}
	return NULL;
}

static int ov7736_detect(struct ov7736_device *dev, struct i2c_client *client)
{

	struct i2c_adapter *adapter = client->adapter;
	u32 pid_h, pid_l;
	u32 retvalue;

	dev_info(&client->dev, "%s: client->addr = %x\n", __func__,
			client->addr);
	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "%s: i2c error", __func__);
		return -ENODEV;
	}
	dev_info(&client->dev, "%s: I2C read enter client->addr = %x\n",
			__func__, client->addr);
	ov7736_read_reg(client, MISENSOR_8BIT, 0x300A, &pid_h);
	ov7736_read_reg(client, MISENSOR_8BIT, 0x300B, &pid_l);
	dev_info(&client->dev, "%s: I2C read exit client->addr = %x\n",
			__func__, client->addr);
	dev->real_model_id = (pid_h << 8 | pid_l);
	dev_info(&client->dev, "%s: client->addr = %x,module_ID = 0x%x\n",
			__func__, client->addr, dev->real_model_id);

	if (dev->real_model_id != OV7736_MOD_ID) {
		dev_err(&client->dev, "%s: failed: client->addr = %x\n",
			__func__, client->addr);
		return -ENODEV;
	}

	return 0;
}

static int
ov7736_s_config(struct v4l2_subdev *sd, int irq, void *platform_data)
{
	struct ov7736_device *dev = to_ov7736_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	if (NULL == platform_data)
		return -ENODEV;

	dev->platform_data =
		(struct camera_sensor_platform_data *)platform_data;

	ret = ov7736_s_power(sd, 1);

	ov7736_init_common(sd);

	if (ret) {
		v4l2_err(client, "ov7736 power-up err");
		return ret;
	}

	/* config & detect sensor */
	ret = ov7736_detect(dev, client);
	if (ret) {
		v4l2_err(client, "ov7736_detect err s_config.\n");
		goto fail_detect;
	}

	ret = dev->platform_data->csi_cfg(sd, 1);
	if (ret)
		goto fail_csi_cfg;

	ret = ov7736_s_power(sd, 0);
	if (ret) {
		v4l2_err(client, "ov7736 power down err");
		return ret;
	}

	return 0;

fail_csi_cfg:
	dev->platform_data->csi_cfg(sd, 0);
fail_detect:
	ov7736_s_power(sd, 0);
	dev_err(&client->dev, "sensor power-gating failed\n");
	return ret;
}

static int ov7736_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	struct ov7736_control *ctrl = ov7736_find_control(qc->id);

	if (ctrl == NULL)
		return -EINVAL;
	*qc = ctrl->qc;
	return 0;
}

/* Horizontal flip the image. */
static int ov7736_t_hflip(struct v4l2_subdev *sd, int value)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	struct ov7736_device *dev = to_ov7736_sensor(sd);
	int err;

	pr_debug("%s %d\n", __func__, value);

	/* Since we are flipping image initially in the ov7736_streaming array,
	 * the internal meaning of the value is inverted */
	if (!value) {
		/* enable H flip */
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0x3818, 0x40, 0x01);
		dev->bpat = OV7736_BPAT_GRGRBGBG;
	} else {
		/* disable H  */
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0x3818, 0x40, 0x00);
		dev->bpat = OV7736_BPAT_BGBGGRGR;
	}

	udelay(10);

	return !!err;
}

/* Vertically flip the image */
static int ov7736_t_vflip(struct v4l2_subdev *sd, int value)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	int err;

	pr_debug("%s %d\n", __func__, value);

	/* Since we are flipping image initially in the ov7736_streaming array,
	 * the internal meaning of the value is inverted */
	if (!value) {
		/* enable V flip */
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0x3818, 0x20, 0x01);
	} else {
		/* disable V  */
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0x3818, 0x20, 0x00);
	}

	udelay(10);

	return err;
}

static int ov7736_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct ov7736_control *octrl = ov7736_find_control(ctrl->id);
	int ret;

	if (octrl == NULL)
		return -EINVAL;

	ret = octrl->query(sd, &ctrl->value);
	if (ret < 0)
		return ret;

	return 0;
}

static int ov7736_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct ov7736_control *octrl = ov7736_find_control(ctrl->id);
	int ret;

	if (octrl == NULL)
		return -EINVAL;

	ret = octrl->tweak(sd, ctrl->value);
	if (ret < 0)
		return ret;

	return 0;
}


static int ov7736_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret;
	struct i2c_client *c = v4l2_get_subdevdata(sd);

	pr_debug("%s %d\n", __func__, enable);

	if (enable) {

		ret = ov7736_set_streaming(sd);

		/*
		 * here we wait for sensor's 3A algorithm to be
		 * stablized, as to fix still capture bad 3A output picture
		 */
#ifdef OV7736_WAIT_3A
		if (ov7736_wait_3a(sd))
			v4l2_warn(c, "3A cannot finish!");
#endif
	} else
		ret = ov7736_set_suspend(sd);

	return ret;
}

static int
ov7736_enum_framesizes(struct v4l2_subdev *sd, struct v4l2_frmsizeenum *fsize)
{
	unsigned int index = fsize->index;

	if (index >= N_RES)
		return -EINVAL;

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = ov7736_res[index].width;
	fsize->discrete.height = ov7736_res[index].height;

	/* FIXME: Wrong way to know used mode */
	fsize->reserved[0] = ov7736_res[index].used;

	return 0;
}

static int
ov7736_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct ov7736_device *dev = to_ov7736_sensor(sd);

	if (param->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	memset(param, 0, sizeof(*param));
	param->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (dev->res >= 0 && dev->res < N_RES) {
		param->parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
		param->parm.capture.timeperframe.numerator = 1;
		param->parm.capture.timeperframe.denominator =
				ov7736_res[dev->res].fps;
	}
	return 0;
}

static int
ov7736_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	return ov7736_g_parm(sd, param);
}


static int ov7736_enum_frameintervals(struct v4l2_subdev *sd,
				       struct v4l2_frmivalenum *fival)
{
	unsigned int index = fival->index;
	int i;

	if (index >= N_RES)
		return -EINVAL;

	/* find out the first equal or bigger size */
	for (i = 0; i < N_RES; i++) {
		if ((ov7736_res[i].width >= fival->width) &&
		    (ov7736_res[i].height >= fival->height))
			break;
	}
	if (i == N_RES)
		i--;

	index = i;

	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fival->discrete.numerator = 1;
	fival->discrete.denominator = ov7736_res[index].fps;

	return 0;
}

static int
ov7736_g_chip_ident(struct v4l2_subdev *sd, struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_OV7736, 0);
}

static int ov7736_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_fh *fh,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index >= MAX_FMTS)
		return -EINVAL;
	code->code = V4L2_MBUS_FMT_SGRBG10_1X10;

	return 0;
}

static int ov7736_enum_frame_size(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh,
	struct v4l2_subdev_frame_size_enum *fse)
{

	unsigned int index = fse->index;


	if (index >= N_RES)
		return -EINVAL;

	fse->min_width = ov7736_res[index].width;
	fse->min_height = ov7736_res[index].height;
	fse->max_width = ov7736_res[index].width;
	fse->max_height = ov7736_res[index].height;

	return 0;
}

static struct v4l2_mbus_framefmt *
__ov7736_get_pad_format(struct ov7736_device *sensor,
			 struct v4l2_subdev_fh *fh, unsigned int pad,
			 enum v4l2_subdev_format_whence which)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->sd);

	if (pad != 0) {
		dev_err(&client->dev,  "%s err. pad %x\n", __func__, pad);
		return NULL;
	}

	switch (which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		return v4l2_subdev_get_try_format(fh, pad);
	case V4L2_SUBDEV_FORMAT_ACTIVE:
		return &sensor->format;
	default:
		return NULL;
	}
}

static int
ov7736_get_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct ov7736_device *snr = to_ov7736_sensor(sd);
	struct v4l2_mbus_framefmt *format =
			__ov7736_get_pad_format(snr, fh, fmt->pad, fmt->which);

	if (format == NULL)
		return -EINVAL;
	fmt->format = *format;

	return 0;
}

static int
ov7736_set_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct ov7736_device *snr = to_ov7736_sensor(sd);
	struct v4l2_mbus_framefmt *format =
			__ov7736_get_pad_format(snr, fh, fmt->pad, fmt->which);

	if (format == NULL)
		return -EINVAL;

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		snr->format = fmt->format;

	return 0;
}

static const struct v4l2_subdev_video_ops ov7736_video_ops = {
	.try_mbus_fmt = ov7736_try_mbus_fmt,
	.s_mbus_fmt = ov7736_set_mbus_fmt,
	.g_mbus_fmt = ov7736_get_mbus_fmt,
	.s_stream = ov7736_s_stream,
	.enum_framesizes = ov7736_enum_framesizes,
	.s_parm = ov7736_s_parm,
	.g_parm = ov7736_g_parm,
	.enum_frameintervals = ov7736_enum_frameintervals,
};

static const struct v4l2_subdev_core_ops ov7736_core_ops = {
	.g_chip_ident = ov7736_g_chip_ident,
	.queryctrl = ov7736_queryctrl,
	.g_ctrl = ov7736_g_ctrl,
	.s_ctrl = ov7736_s_ctrl,
	.s_power = ov7736_s_power,
	.ioctl = ov7736_ioctl,
};

/* REVISIT: Do we need pad operations? */
static const struct v4l2_subdev_pad_ops ov7736_pad_ops = {
	.enum_mbus_code = ov7736_enum_mbus_code,
	.enum_frame_size = ov7736_enum_frame_size,
	.get_fmt = ov7736_get_pad_format,
	.set_fmt = ov7736_set_pad_format,
};

static const struct v4l2_subdev_ops ov7736_ops = {
	.core = &ov7736_core_ops,
	.video = &ov7736_video_ops,
	.pad = &ov7736_pad_ops,
};

static const struct media_entity_operations ov7736_entity_ops = {
/*	.set_power = v4l2_subdev_set_power,*/
};


static int ov7736_remove(struct i2c_client *client)
{
	struct ov7736_device *dev;
	struct v4l2_subdev *sd = i2c_get_clientdata(client);

	dev = container_of(sd, struct ov7736_device, sd);
	dev->platform_data->csi_cfg(sd, 0);

	v4l2_device_unregister_subdev(sd);
	media_entity_cleanup(&dev->sd.entity);
	kfree(dev);

	return 0;
}

static int ov7736_probe(struct i2c_client *client,
		       const struct i2c_device_id *id)
{
	struct ov7736_device *dev;
	int ret;

	pr_debug("%s\n", __func__);

	/* Setup sensor configuration structure */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&client->dev, "out of memory\n");
		return -ENOMEM;
	}

	v4l2_i2c_subdev_init(&dev->sd, client, &ov7736_ops);
	if (client->dev.platform_data) {
		ret = ov7736_s_config(&dev->sd, client->irq,
				       client->dev.platform_data);
		if (ret) {
			v4l2_device_unregister_subdev(&dev->sd);
			kfree(dev);
			return ret;
		}
	}

	/*TODO add format code here*/
	dev->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->pad.flags = MEDIA_PAD_FL_SOURCE;

	/* REVISIT: Do we need media controller? */
	ret = media_entity_init(&dev->sd.entity, 1, &dev->pad, 0);
	if (ret) {
		ov7736_remove(client);
		return ret;
	}

	/* set res index to be invalid */
	dev->res = -1;

	return 0;
}

MODULE_DEVICE_TABLE(i2c, ov7736_id);
static struct i2c_driver ov7736_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "ov7736"
	},
	.probe = ov7736_probe,
	.remove = ov7736_remove,
	.id_table = ov7736_id,
};

static __init int init_ov7736(void)
{
	return i2c_add_driver(&ov7736_driver);
}

static __exit void exit_ov7736(void)
{
	i2c_del_driver(&ov7736_driver);
}

module_init(init_ov7736);
module_exit(exit_ov7736);


MODULE_AUTHOR("Foxconn Technology Group");
MODULE_LICENSE("GPL");
