/*
 * Support for Medifield PNW Camera Imaging ISP subsystem.
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
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>

#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>

#include <linux/io.h>

#include "lm3556.h"

#define to_lm3556_priv(p_sd) \
	container_of(p_sd, struct lm3556_priv, sd)

#define INIT_FIELD(_reg_address, _lsb, _msb) { \
	.reg_address = _reg_address, \
	.lsb = _lsb, \
	.msb = _msb \
}

struct lm3556_ctrl_id {
	struct v4l2_queryctrl qc;
	int (*s_ctrl) (struct v4l2_subdev *sd, __u32 val);
	int (*g_ctrl) (struct v4l2_subdev *sd, __u32 *val);
};

struct lm3556_priv {
	struct v4l2_subdev sd;
	struct mutex i2c_mutex;
	enum atomisp_flash_mode mode;
	int timeout;
};

struct lm3556_reg_field {
	u32 reg_address;
	u32 lsb;
	u32 msb;
};

/* This defines the register field properties. The LSBs and MSBs come from
 * the lm3556 datasheet. */
static const struct lm3556_reg_field
	enable             = INIT_FIELD(LM3556_ENABLE_REG,           0, 7),
	configure          = INIT_FIELD(LM3556_CONFIGURATION_REG,    0, 7),
	torch_current      = INIT_FIELD(LM3556_CURRENT_CONTROL_REG,  4, 6),
	indicator_current  = INIT_FIELD(LM3556_FLASH_FEATURES_REG,   6, 7),
	flash_current      = INIT_FIELD(LM3556_CURRENT_CONTROL_REG,  0, 3),
	flash_timeout      = INIT_FIELD(LM3556_FLASH_FEATURES_REG,   0, 2),
	flags              = INIT_FIELD(LM3556_FLAGS_REG,            0, 7);

static int set_reg_field(struct v4l2_subdev *sd,
			 const struct lm3556_reg_field *field,
			 u8 value)
{
	int ret;
	u32 tmp,
	    val = value,
	    bits = (field->msb - field->lsb) + 1,
	    mask = ((1<<bits)-1) << field->lsb;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct lm3556_priv *p_lm3556_priv = to_lm3556_priv(sd);

	mutex_lock(&p_lm3556_priv->i2c_mutex);
	tmp = i2c_smbus_read_byte_data(client, field->reg_address);
	tmp &= ~mask;
	val = (val << field->lsb) & mask;
	ret = i2c_smbus_write_byte_data(client, field->reg_address, val | tmp);
	mutex_unlock(&p_lm3556_priv->i2c_mutex);

	if (ret < 0)
		dev_err(&client->dev, "%s: flash i2c fail", __func__);

	return ret;
}

static void get_reg_field(struct v4l2_subdev *sd,
			  const struct lm3556_reg_field *field,
			  u8 *value)
{
	u32 tmp,
	    bits = (field->msb - field->lsb) + 1,
	    mask = ((1<<bits)-1) << field->lsb;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct lm3556_priv *p_lm3556_priv = to_lm3556_priv(sd);

	mutex_lock(&p_lm3556_priv->i2c_mutex);
	tmp = i2c_smbus_read_byte_data(client, field->reg_address);
	mutex_unlock(&p_lm3556_priv->i2c_mutex);

	*value = (tmp & mask) >> field->lsb;
}

static int lm3556_hw_reset(struct i2c_client *client)
{
	int ret = 0;

	gpio_direction_output(GP_LM3556_FLASH_RESET, 0);
	msleep(50);
	gpio_direction_output(GP_LM3556_FLASH_RESET, 1);
	msleep(50);

	return ret;
}

static int lm3556_s_flash_timeout(struct v4l2_subdev *sd, u32 val)
{
	struct lm3556_priv *p_lm3556 = to_lm3556_priv(sd);

	pr_debug("%s %d\n", __func__, val);

	val = clamp(val, LM3556_MIN_TIMEOUT, LM3556_MAX_TIMEOUT);
	p_lm3556->timeout = val;

	val = val / LM3556_TIMEOUT_STEPSIZE - 1;
	return set_reg_field(sd, &flash_timeout, (u8)val);
}

static int lm3556_g_flash_timeout(struct v4l2_subdev *sd, u32 *val)
{
	u8 value;

	get_reg_field(sd, &flash_timeout, &value);
	*val = (u32)(value + 1) * LM3556_TIMEOUT_STEPSIZE;

	return 0;
}

static int lm3556_s_flash_intensity(struct v4l2_subdev *sd, u32 intensity)
{
	pr_debug("%s %d\n", __func__, intensity);

	intensity = LM3556_CLAMP_PERCENTAGE(intensity);
	intensity = LM3556_PERCENT_TO_VALUE(intensity, LM3556_FLASH_STEP);

	if (intensity > LM3556_FLASH_MAX_LVL)
		intensity = LM3556_FLASH_MAX_LVL;

	return set_reg_field(sd, &flash_current, (u8)intensity);
}

static int lm3556_g_flash_intensity(struct v4l2_subdev *sd, u32 *val)
{
	u8 value;

	get_reg_field(sd, &flash_current, &value);
	*val = LM3556_VALUE_TO_PERCENT((u32)value, LM3556_FLASH_STEP);

	return 0;
}

static int lm3556_s_torch_intensity(struct v4l2_subdev *sd, u32 intensity)
{
	pr_debug("%s %d\n", __func__, intensity);

	intensity = LM3556_CLAMP_PERCENTAGE(intensity);
	intensity = LM3556_PERCENT_TO_VALUE(intensity, LM3556_TORCH_STEP);


	if (intensity > LM3556_TORCH_MAX_LVL)
		intensity = LM3556_TORCH_MAX_LVL;

	return set_reg_field(sd, &torch_current, (u8)intensity);
}

static int lm3556_g_torch_intensity(struct v4l2_subdev *sd, u32 *val)
{
	u8 value;

	get_reg_field(sd, &torch_current, &value);
	*val = LM3556_VALUE_TO_PERCENT((u32)value, LM3556_TORCH_STEP);

	return 0;
}

static int lm3556_s_indicator_intensity(struct v4l2_subdev *sd, u32 intensity)
{
	pr_debug("%s %d\n", __func__, intensity);

	intensity = LM3556_CLAMP_PERCENTAGE(intensity);
	intensity = LM3556_PERCENT_TO_VALUE(intensity, LM3556_INDICATOR_STEP);

	return set_reg_field(sd, &indicator_current, (u8)intensity);
}

static int lm3556_g_indicator_intensity(struct v4l2_subdev *sd, u32 *val)
{
	u8 value;

	get_reg_field(sd, &indicator_current, &value);
	*val = LM3556_VALUE_TO_PERCENT((u32)value, LM3556_INDICATOR_STEP);

	return 0;
}

static int lm3556_s_flash_strobe(struct v4l2_subdev *sd, u32 val)
{
	int ret;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct lm3556_priv *p_lm3556_priv = to_lm3556_priv(sd);

	pr_debug("%s %d", __func__, val);

	ret = gpio_direction_output(GP_LM3556_FLASH_STROBE, val);
	if (ret < 0) {
		dev_err(&client->dev, "failed to generate flash strobe (%d)\n",
			ret);
		return ret;
	}

	if (val == 0 && p_lm3556_priv->mode == ATOMISP_FLASH_MODE_OFF) {
		/* Driver requires mode bits to get reset after flash event */
		usleep_range(5000, 5000);
		ret =  set_reg_field(sd, &enable, LM3556_MODE_FLASH);
		if (ret < 0) {
			dev_err(&client->dev, "failed to reset flash mode"
					"(%d)\n", ret);
			return ret;
		}
	}

	return 0;
}

static int lm3556_s_flash_mode(struct v4l2_subdev *sd, u32 val)
{
	int ret;
	enum atomisp_flash_mode new_mode = (enum atomisp_flash_mode)val;
	struct lm3556_priv *p_lm3556_priv = to_lm3556_priv(sd);

	pr_debug("%s %d", __func__, val);

	switch (new_mode) {
	case ATOMISP_FLASH_MODE_OFF:
		/* FIXME: controller isn't setting flash mode for flash
		ret = set_reg_field(sd, &enable, LM3556_MODE_SHUTDOWN);
		break;
		*/
	case ATOMISP_FLASH_MODE_FLASH:
		ret = set_reg_field(sd, &enable, LM3556_MODE_FLASH);
		break;
	case ATOMISP_FLASH_MODE_INDICATOR:
		ret = set_reg_field(sd, &enable, LM3556_MODE_INDICATOR);
		break;
	case ATOMISP_FLASH_MODE_TORCH:
		ret = set_reg_field(sd, &enable, LM3556_MODE_TORCH);
		break;
	default:
		ret = -EINVAL;
	}
	if (ret == 0)
		p_lm3556_priv->mode = new_mode;
	return ret;
}

static int lm3556_g_flash_mode(struct v4l2_subdev *sd, u32 * val)
{
	struct lm3556_priv *p_lm3556_priv = to_lm3556_priv(sd);
	*val = p_lm3556_priv->mode;
	return 0;
}

static int lm3556_g_flash_status(struct v4l2_subdev *sd, u32 *val)
{
	u8 value;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	get_reg_field(sd, &flags, &value);

	if (value & LM3556_FLAG_TIMEOUT)
		*val = ATOMISP_FLASH_STATUS_TIMEOUT;
	else if (value & LM3556_FLAG_TX_EVENT) {
		*val = ATOMISP_FLASH_STATUS_INTERRUPTED;
	} else if (value & LM3556_FLAG_THERMAL_SHUTDOWN ||
			value & LM3556_FLAG_LED_FAULT ||
			value & LM3556_FLAG_OVP ||
			value & LM3556_FLAG_UVLO) {
		*val = ATOMISP_FLASH_STATUS_HW_ERROR;
	} else
		*val = ATOMISP_FLASH_STATUS_OK;

	if (*val == ATOMISP_FLASH_STATUS_HW_ERROR)
		dev_err(&client->dev, "LM3556 flag status: %d\n", value);
	return 0;
}

static const struct lm3556_ctrl_id lm3556_ctrls[] = {
	s_ctrl_id_entry_integer(V4L2_CID_FLASH_TIMEOUT,
				"Flash Timeout",
				0,
				LM3556_MAX_TIMEOUT,
				1,
				LM3556_DEFAULT_TIMEOUT,
				0,
				lm3556_s_flash_timeout,
				lm3556_g_flash_timeout),
	s_ctrl_id_entry_integer(V4L2_CID_FLASH_INTENSITY,
				"Flash Intensity",
				LM3556_MIN_PERCENT,
				LM3556_MAX_PERCENT,
				1,
				LM3556_FLASH_DEFAULT_BRIGHTNESS,
				0,
				lm3556_s_flash_intensity,
				lm3556_g_flash_intensity),
	s_ctrl_id_entry_integer(V4L2_CID_FLASH_TORCH_INTENSITY,
				"Torch Intensity",
				LM3556_MIN_PERCENT,
				LM3556_MAX_PERCENT,
				1,
				LM3556_TORCH_DEFAULT_BRIGHTNESS,
				0,
				lm3556_s_torch_intensity,
				lm3556_g_torch_intensity),
	s_ctrl_id_entry_integer(V4L2_CID_FLASH_INDICATOR_INTENSITY,
				"Indicator Intensity",
				LM3556_MIN_PERCENT,
				LM3556_MAX_PERCENT,
				1,
				LM3556_INDICATOR_DEFAULT_BRIGHTNESS,
				0,
				lm3556_s_indicator_intensity,
				lm3556_g_indicator_intensity),
	s_ctrl_id_entry_boolean(V4L2_CID_FLASH_STROBE,
				"Flash Strobe",
				0,
				0,
				lm3556_s_flash_strobe,
				NULL),
	s_ctrl_id_entry_integer(V4L2_CID_FLASH_MODE,
				"Flash Mode",
				0,   /* don't assume any enum ID is first */
				100, /* enum value, may get extended */
				1,
				ATOMISP_FLASH_MODE_OFF,
				0,
				lm3556_s_flash_mode,
				lm3556_g_flash_mode),
	s_ctrl_id_entry_integer(V4L2_CID_FLASH_STATUS,
				"Flash Status",
				0,   /* don't assume any enum ID is first */
				100, /* enum value, may get extended */
				1,
				ATOMISP_FLASH_STATUS_OK,
				0,
				NULL,
				lm3556_g_flash_status),
};

static const struct lm3556_ctrl_id *find_ctrl_id(unsigned int id)
{
	int i;
	int num;

	num = ARRAY_SIZE(lm3556_ctrls);
	for (i = 0; i < num; i++) {
		if (lm3556_ctrls[i].qc.id == id)
			return &lm3556_ctrls[i];
	}

	return NULL;
}

static int lm3556_g_chip_ident(struct v4l2_subdev *sd,
			  struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	chip->ident = LEDFLASH_LM3556_ID;
	chip->revision = 0;
	chip->match.type = V4L2_CHIP_MATCH_I2C_DRIVER;
	chip->match.addr = client->addr;
	strlcpy(chip->match.name, client->name, strlen(client->name));

	return 0;
}

static int lm3556_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	struct i2c_client *client;
	int num;

	client = v4l2_get_subdevdata(sd);

	if (!qc)
		return -EINVAL;

	num = ARRAY_SIZE(lm3556_ctrls);
	if (qc->id >= num)
		return -EINVAL;

	*qc = lm3556_ctrls[qc->id].qc;

	return 0;
}

static int lm3556_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	const struct lm3556_ctrl_id *s_ctrl;

	if (!ctrl)
		return -EINVAL;

	s_ctrl = find_ctrl_id(ctrl->id);
	if (!s_ctrl)
		return -EINVAL;

	return s_ctrl->s_ctrl(sd, ctrl->value);
}

static int lm3556_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	const struct lm3556_ctrl_id *s_ctrl;

	if (!ctrl)
		return -EINVAL;

	s_ctrl = find_ctrl_id(ctrl->id);
	if (s_ctrl == NULL)
		return -EINVAL;

	return s_ctrl->g_ctrl(sd, &ctrl->value);
}

static int lm3556_s_power(struct v4l2_subdev *sd, int power)
{
	sd = sd;
	power = power;
	return 0;
}

static const struct v4l2_subdev_core_ops lm3556_core_ops = {
	.g_chip_ident = lm3556_g_chip_ident,
	.queryctrl = lm3556_queryctrl,
	.g_ctrl = lm3556_g_ctrl,
	.s_ctrl = lm3556_s_ctrl,
	.s_power = lm3556_s_power,
};

static const struct v4l2_subdev_ops lm3556_ops = {
	.core = &lm3556_core_ops,
};

static const struct media_entity_operations lm3556_entity_ops = {
/*	.set_power = v4l2_subdev_set_power,*/
};

static const struct i2c_device_id lm3556_id[] = {
	{LEDFLASH_LM3556_NAME, 0},
	{},
};

static int lm3556_detect(struct i2c_client *client)
{
	u32 status;
	struct i2c_adapter *adapter = client->adapter;
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	int ret;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev, "lm3556_detect i2c error\n");
		return -ENODEV;
	}

	lm3556_hw_reset(client);

	ret = gpio_direction_output(GP_LM3556_FLASH_STROBE, 0);
	if (ret < 0)
		goto fail;

	ret = set_reg_field(sd, &configure, 0xF8);
	if (ret < 0)
		goto fail;

	/* clear the flags register */
	ret = lm3556_g_flash_status(sd, &status);
	if (ret < 0)
		goto fail;

	dev_dbg(&client->dev, "Successfully detected lm3556 LED flash\n");
	return 0;

fail:
	dev_err(&client->dev, "lm3556_detect fail........");
	return ret;
}

static int __devinit lm3556_probe(struct i2c_client *client,
					 const struct i2c_device_id *id)
{
	int err;
	struct lm3556_priv *p_lm3556_priv;

	p_lm3556_priv = kzalloc(sizeof(*p_lm3556_priv), GFP_KERNEL);
	if (!p_lm3556_priv) {
		dev_err(&client->dev, "out of memory\n");
		return -ENOMEM;
	}

	v4l2_i2c_subdev_init(&(p_lm3556_priv->sd), client, &lm3556_ops);
	p_lm3556_priv->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	p_lm3556_priv->sd.entity.ops = &lm3556_entity_ops;
	p_lm3556_priv->timeout = LM3556_DEFAULT_TIMEOUT;
	p_lm3556_priv->mode = ATOMISP_FLASH_MODE_OFF;

	err = media_entity_init(&p_lm3556_priv->sd.entity, 0, NULL, 0);
	if (err) {
		dev_err(&client->dev, "error initialize a media entity.\n");
		goto fail1;
	}

	mutex_init(&p_lm3556_priv->i2c_mutex);

	err = gpio_request(GP_LM3556_FLASH_STROBE, "lm3556 strobe");
	if (err) {
		dev_err(&client->dev, "unable to claim lm3556 strobe gpio\n");
		goto fail2;
	}
	err = gpio_export(GP_LM3556_FLASH_STROBE, 0);
	if (err) {
		dev_err(&client->dev, "unable to export lm3556 strobe gpio\n");
		goto fail3;
	}

	err = gpio_request(GP_LM3556_FLASH_RESET, "lm3556 reset");
	if (err) {
		dev_err(&client->dev, "unable to claim flash lm3556 reset gpio\n");
		goto fail3;
	}
	err = gpio_export(GP_LM3556_FLASH_RESET, 0);
	if (err) {
		dev_err(&client->dev, "unable to export lm3556 reset gpio\n");
		goto fail4;
	}

	err = lm3556_detect(client);
	if (err) {
		dev_err(&client->dev, "device not found\n");
		goto fail4;
	}

	return 0;

fail4:
	gpio_free(GP_LM3556_FLASH_RESET);
fail3:
	gpio_free(GP_LM3556_FLASH_STROBE);
fail2:
	media_entity_cleanup(&p_lm3556_priv->sd.entity);
fail1:
	v4l2_device_unregister_subdev(&p_lm3556_priv->sd);
	kfree(p_lm3556_priv);

	return err;
}

static int lm3556_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct lm3556_priv *p_lm3556_priv = to_lm3556_priv(sd);
	int ret;

	media_entity_cleanup(&p_lm3556_priv->sd.entity);
	v4l2_device_unregister_subdev(sd);

	ret = gpio_direction_output(GP_LM3556_FLASH_STROBE, 0);
	if (ret < 0)
		dev_err(&client->dev, "failed to set strobe on remove");

	ret = gpio_direction_output(GP_LM3556_FLASH_RESET, 0);
	if (ret < 0)
		dev_err(&client->dev, "failed to set reset on remove");

	gpio_free(GP_LM3556_FLASH_RESET);
	gpio_free(GP_LM3556_FLASH_STROBE);

	kfree(p_lm3556_priv);

	return 0;
}

MODULE_DEVICE_TABLE(i2c, lm3556_id);

static struct i2c_driver lm3556_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = LEDFLASH_LM3556_NAME,
	},
	.probe = lm3556_probe,
	.remove = lm3556_remove,
	.id_table = lm3556_id,
};

static __init int init_lm3556(void)
{
	return i2c_add_driver(&lm3556_driver);
}

static __exit void exit_lm3556(void)
{
	i2c_del_driver(&lm3556_driver);
}

module_init(init_lm3556);
module_exit(exit_lm3556);
MODULE_AUTHOR("Jing Tao <jing.tao@intel.com>");
MODULE_LICENSE("GPL");
