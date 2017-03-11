/*
 * Copyright (C) 2011-2012 Motorola Mobility, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/ct406.h>

#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>

#define CT406_I2C_RETRIES	5
#define CT406_I2C_RETRY_DELAY	10

#define CT406_COMMAND_SELECT		0x80
#define CT406_COMMAND_AUTO_INCREMENT	0x20
#define CT406_COMMAND_SPECIAL_FUNCTION	0x60
#define CT406_COMMAND_PROX_INT_CLEAR	0x05
#define CT406_COMMAND_ALS_INT_CLEAR	0x06

#define CT406_ENABLE			0x00
#define CT406_ENABLE_PIEN		(1<<5)
#define CT406_ENABLE_AIEN		(1<<4)
#define CT406_ENABLE_WEN		(1<<3)
#define CT406_ENABLE_PEN		(1<<2)
#define CT406_ENABLE_AEN		(1<<1)
#define CT406_ENABLE_PON		(1<<0)

#define CT406_ATIME			0x01
#define CT406_ATIME_SATURATED		0xFF
#define CT406_ATIME_NOT_SATURATED	0xEE

#define CT406_PTIME			0x02

#define CT406_WTIME			0x03
#define CT406_WTIME_ALS_OFF		0xEE
#define CT406_WTIME_ALS_ON		0xFF
#define CT406_WTIME_SATURATED		0xFF

#define CT406_AILTL			0x04
#define CT406_AILTH			0x05
#define CT406_AIHTL			0x06
#define CT406_AIHTH			0x07
#define CT406_PILTL			0x08
#define CT406_PILTH			0x09
#define CT406_PIHTL			0x0A
#define CT406_PIHTH			0x0B

#define CT406_PERS			0x0C
#define CT406_PERS_PPERS_MASK		0xF0
#define CT406_PERS_APERS_MASK		0x0F
#define CT406_PERS_PPERS		0x10
#define CT406_PERS_APERS_SATURATED	0x03

#define CT406_CONFIG			0x0D
#define CT406_CONFIG_AGL		(1<<2)

#define CT406_PPCOUNT			0x0E

#define CT406_CONTROL			0x0F
#define CT406_CONTROL_PDIODE_CH0	0x10
#define CT406_CONTROL_PDIODE_CH1	0x20
#define CT406_CONTROL_PGAIN_1X		0x00
#define CT406_CONTROL_PGAIN_2X		0x04
#define CT406_CONTROL_PGAIN_4X		0x08
#define CT406_CONTROL_PGAIN_8X		0x0C
#define CT406_CONTROL_AGAIN_1X		0x00
#define CT406_CONTROL_AGAIN_8X		0x01
#define CT406_CONTROL_AGAIN_16X		0x02
#define CT406_CONTROL_AGAIN_120X	0x03

#define CT406_REV_ID			0x11

#define CT406_ID			0x12

#define CT406_STATUS			0x13
#define CT406_STATUS_PINT		(1<<5)
#define CT406_STATUS_AINT		(1<<4)

#define CT406_C0DATA			0x14
#define CT406_C0DATAH			0x15
#define CT406_C1DATA			0x16
#define CT406_C1DATAH			0x17
#define CT406_PDATA			0x18
#define CT406_PDATAH			0x19
#define CT406_POFFSET			0x1E

#define CT406_C0DATA_MAX		0xFFFF
#define CT405_PDATA_MAX			0x03FF
#define CT406_PDATA_MAX			0x07FF

#define CT406_PROXIMITY_NEAR		30	/* 30 mm */
#define CT406_PROXIMITY_FAR		1000	/* 1 meter */

#define CT406_ALS_LOW_TO_HIGH_THRESHOLD	200	/* 200 lux */
#define CT406_ALS_HIGH_TO_LOW_THRESHOLD	100	/* 100 lux */

#define CT40X_REV_ID_CT405		0x02
#define CT40X_REV_ID_CT406a		0x03
#define CT40X_REV_ID_CT406b		0x04

enum ct406_prox_mode {
	CT406_PROX_MODE_SATURATED,
	CT406_PROX_MODE_UNCOVERED,
	CT406_PROX_MODE_COVERED,
};

enum ct406_als_mode {
	CT406_ALS_MODE_SUNLIGHT,
	CT406_ALS_MODE_LOW_LUX,
	CT406_ALS_MODE_HIGH_LUX,
};

enum ct40x_hardware_type {
	CT405_HW_TYPE,
	CT406_HW_TYPE,
};

struct ct406_data {
	struct input_dev *dev;
	struct i2c_client *client;
	struct regulator *regulator;
	struct work_struct work;
	struct workqueue_struct *workqueue;
	struct ct406_platform_data *pdata;
	struct miscdevice miscdevice;
	struct notifier_block pm_notifier;
	struct mutex mutex;
	/* state flags */
	unsigned int suspended;
	unsigned int regs_initialized;
	unsigned int oscillator_enabled;
	unsigned int prox_requested;
	unsigned int prox_enabled;
	enum ct406_prox_mode prox_mode;
	unsigned int als_requested;
	unsigned int als_enabled;
	unsigned int als_apers;
	unsigned int als_first_report;
	enum ct406_als_mode als_mode;
	unsigned int ip_requested;
	unsigned int ip_enabled;
	unsigned int wait_enabled;
	/* numeric values */
	unsigned int prox_noise_floor;
	unsigned int prox_low_threshold;
	unsigned int prox_high_threshold;
	unsigned int als_low_threshold;
	u16 prox_saturation_threshold;
	u16 prox_covered_offset;
	u16 prox_uncovered_offset;
	u16 prox_recalibrate_offset;
	u8 prox_pulse_count;
	u8 prox_offset;
	u16 pdata_max;
	enum ct40x_hardware_type hw_type;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend ct406_early_suspend;
#endif
};

static struct ct406_data *ct406_misc_data;

static struct ct406_reg {
	const char *name;
	u8 reg;
} ct406_regs[] = {
	{ "ENABLE",	CT406_ENABLE },
	{ "ATIME",	CT406_ATIME },
	{ "PTIME",	CT406_PTIME },
	{ "WTIME",	CT406_WTIME },
	{ "AILTL",	CT406_AILTL },
	{ "AILTH",	CT406_AILTH },
	{ "AIHTL",	CT406_AIHTL },
	{ "AIHTH",	CT406_AIHTH },
	{ "PILTL",	CT406_PILTL },
	{ "PILTH",	CT406_PILTH },
	{ "PIHTL",	CT406_PIHTL },
	{ "PIHTH",	CT406_PIHTH },
	{ "PERS",	CT406_PERS },
	{ "CONFIG",	CT406_CONFIG },
	{ "PPCOUNT",	CT406_PPCOUNT },
	{ "CONTROL",	CT406_CONTROL },
	{ "ID",		CT406_ID },
	{ "STATUS",	CT406_STATUS },
	{ "C0DATA",	CT406_C0DATA },
	{ "C0DATAH",	CT406_C0DATAH },
	{ "C1DATA",	CT406_C1DATA },
	{ "C1DATAH",	CT406_C1DATAH },
	{ "PDATA",	CT406_PDATA },
	{ "PDATAH",	CT406_PDATAH },
	{ "POFFSET",	CT406_POFFSET },
};

#define CT406_DBG_INPUT			0x00000001
#define CT406_DBG_POWER_ON_OFF		0x00000002
#define CT406_DBG_ENABLE_DISABLE	0x00000004
#define CT406_DBG_IOCTL			0x00000008
#define CT406_DBG_SUSPEND_RESUME	0x00000010
static u32 ct406_debug = 0x00000000;
module_param_named(debug_mask, ct406_debug, uint, 0644);

static int ct406_i2c_read(struct ct406_data *ct, u8 *buf, int len)
{
	int err;
	int tries = 0;
	struct i2c_msg msgs[] = {
		{
			.addr = ct->client->addr,
			.flags = ct->client->flags & I2C_M_TEN,
			.len = 1,
			.buf = buf,
		},
		{
			.addr = ct->client->addr,
			.flags = (ct->client->flags & I2C_M_TEN) | I2C_M_RD,
			.len = len,
			.buf = buf,
		},
	};

	buf[0] |= CT406_COMMAND_SELECT;
	do {
		err = i2c_transfer(ct->client->adapter, msgs, 2);
		if (err != 2)
			msleep_interruptible(CT406_I2C_RETRY_DELAY);
	} while ((err != 2) && (++tries < CT406_I2C_RETRIES));

	if (err != 2) {
		pr_err("%s: read transfer error.\n", __func__);
		dev_err(&ct->client->dev, "read transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int ct406_i2c_write(struct ct406_data *ct, u8 *buf, int len)
{
	int err;
	int tries = 0;
	struct i2c_msg msgs[] = {
		{
			.addr = ct->client->addr,
			.flags = ct->client->flags & I2C_M_TEN,
			.len = len + 1,
			.buf = buf,
		},
	};

	buf[0] |= CT406_COMMAND_SELECT;
	do {
		err = i2c_transfer(ct->client->adapter, msgs, 1);
		if (err != 1)
			msleep_interruptible(CT406_I2C_RETRY_DELAY);
	} while ((err != 1) && (++tries < CT406_I2C_RETRIES));

	if (err != 1) {
		pr_err("%s: write transfer error.\n", __func__);
		dev_err(&ct->client->dev, "write transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int ct406_write_enable(struct ct406_data *ct)
{
	int error = 0;
	u8 reg_data[2] = {0x00, 0x00};
	reg_data[0] = CT406_ENABLE;
	if (ct->oscillator_enabled || ct->als_enabled || ct->prox_enabled) {
		reg_data[1] |= CT406_ENABLE_PON;
		if (!ct->oscillator_enabled) {
			error = ct406_i2c_write(ct, reg_data, 1);
			if (error < 0)
				return error;
			usleep_range(3000, 3100);
			ct->oscillator_enabled = 1;
		}
		if (ct->als_enabled)
			reg_data[1] |= CT406_ENABLE_AEN | CT406_ENABLE_AIEN;
		if (ct->prox_enabled)
			reg_data[1] |= CT406_ENABLE_PEN | CT406_ENABLE_PIEN;
		if (ct->wait_enabled)
			reg_data[1] |= CT406_ENABLE_WEN;
	}
	if (ct406_debug & CT406_DBG_ENABLE_DISABLE)
		pr_info("%s: writing ENABLE=0x%02x\n", __func__, reg_data[1]);

	return ct406_i2c_write(ct, reg_data, 1);
}

static int ct406_set_als_enable(struct ct406_data *ct,
				unsigned int enable)
{
	int error = 0;
	if (ct->als_enabled != enable) {
		ct->als_enabled = enable;
		if (ct->regs_initialized)
			error = ct406_write_enable(ct);
	}
	return error;
}

static int ct406_set_prox_enable(struct ct406_data *ct,
				 unsigned int enable)
{
	int error = 0;
	if (ct->prox_enabled != enable) {
		ct->prox_enabled = enable;
		if (ct->regs_initialized)
			error = ct406_write_enable(ct);
	}
	return error;
}

static int ct406_set_wait_enable(struct ct406_data *ct,
				 unsigned int enable)
{
	int error = 0;
	if (ct->wait_enabled != enable) {
		ct->wait_enabled = enable;
		if (ct->regs_initialized)
			error = ct406_write_enable(ct);
	}
	return error;
}

static int ct406_clear_als_flag(struct ct406_data *ct)
{
	u8 reg_data[1] = {0};
	reg_data[0] = CT406_COMMAND_SPECIAL_FUNCTION
		| CT406_COMMAND_ALS_INT_CLEAR;
	return ct406_i2c_write(ct, reg_data, 0);
}

static int ct406_clear_prox_flag(struct ct406_data *ct)
{
	u8 reg_data[1] = {0};
	reg_data[0] = CT406_COMMAND_SPECIAL_FUNCTION
		| CT406_COMMAND_PROX_INT_CLEAR;
	return ct406_i2c_write(ct, reg_data, 0);
}

static int ct406_init_registers(struct ct406_data *ct)
{
	int error = 0;
	u8 reg_data[3] = {0};

	/* write ALS integration time = ~49 ms */
	/* write prox integration time = ~3 ms */
	reg_data[0] = (CT406_ATIME | CT406_COMMAND_AUTO_INCREMENT);
	reg_data[1] = CT406_ATIME_NOT_SATURATED;
	if (ct->hw_type == CT405_HW_TYPE)
		reg_data[2] = 0xFF; /* 2.73 ms */
	else
		reg_data[2] = 0xFE; /* 5.46 ms */
	error = ct406_i2c_write(ct, reg_data, 2);
	if (error < 0)
		return error;

	/* write IR LED pulse count */
	reg_data[0] = (CT406_PPCOUNT | CT406_COMMAND_AUTO_INCREMENT);
	reg_data[1] = ct->prox_pulse_count;
	error = ct406_i2c_write(ct, reg_data, 1);
	if (error < 0)
		return error;

	/* write proximity diode = ch1, proximity gain = 1/2, ALS gain = 1 */
	reg_data[0] = (CT406_CONTROL | CT406_COMMAND_AUTO_INCREMENT);
	if (ct->hw_type == CT405_HW_TYPE)
		reg_data[1] = CT406_CONTROL_PDIODE_CH1
			| CT406_CONTROL_PGAIN_2X | CT406_CONTROL_AGAIN_1X;
	else
		reg_data[1] = CT406_CONTROL_PDIODE_CH1
			| CT406_CONTROL_PGAIN_1X | CT406_CONTROL_AGAIN_1X;
	error = ct406_i2c_write(ct, reg_data, 1);
	if (error < 0)
		return error;

	/* write proximity offset */
	if (ct->hw_type != CT405_HW_TYPE) {
		reg_data[0] = (CT406_POFFSET | CT406_COMMAND_AUTO_INCREMENT);
		reg_data[1] = ct->prox_offset;
		error = ct406_i2c_write(ct, reg_data, 1);
		if (error < 0)
			return error;
	}

	return 0;
}

static void ct406_write_als_thresholds(struct ct406_data *ct)
{
	u8 reg_data[5] = {0};
	unsigned int ailt = ct->als_low_threshold;
	unsigned int aiht = CT406_C0DATA_MAX;
	int error;

	reg_data[0] = (CT406_AILTL | CT406_COMMAND_AUTO_INCREMENT);
	reg_data[1] = (ailt & 0xFF);
	reg_data[2] = ((ailt >> 8) & 0xFF);
	reg_data[3] = (aiht & 0xFF);
	reg_data[4] = ((aiht >> 8) & 0xFF);

	error = ct406_i2c_write(ct, reg_data, 4);
	if (error < 0)
		pr_err("%s: Error writing new ALS thresholds: %d\n",
			__func__, error);
}

static void ct406_als_mode_sunlight(struct ct406_data *ct)
{
	int error;
	u8 reg_data[2] = {0};

	/* set AGL to reduce ALS gain by 1/6 */
	reg_data[0] = CT406_CONFIG;
	reg_data[1] = CT406_CONFIG_AGL;
	error = ct406_i2c_write(ct, reg_data, 1);
	if (error < 0) {
		pr_err("%s: error writing CONFIG: %d\n", __func__, error);
		return;
	}

	/* write ALS gain = 1 */
	reg_data[0] = CT406_CONTROL;
	if (ct->hw_type == CT405_HW_TYPE)
		reg_data[1] = CT406_CONTROL_PDIODE_CH1
			| CT406_CONTROL_PGAIN_2X | CT406_CONTROL_AGAIN_1X;
	else
		reg_data[1] = CT406_CONTROL_PDIODE_CH1
			| CT406_CONTROL_PGAIN_1X | CT406_CONTROL_AGAIN_1X;
	error = ct406_i2c_write(ct, reg_data, 1);
	if (error < 0) {
		pr_err("%s: error writing ALS gain: %d\n", __func__, error);
		return;
	}

	/* write ALS integration time = ~3 ms */
	reg_data[0] = CT406_ATIME;
	reg_data[1] = CT406_ATIME_SATURATED;
	error = ct406_i2c_write(ct, reg_data, 1);
	if (error < 0) {
		pr_err("%s: error writing ATIME: %d\n", __func__, error);
		return;
	}

	ct->als_mode = CT406_ALS_MODE_SUNLIGHT;
	ct->als_low_threshold = ct->prox_saturation_threshold;
	ct406_write_als_thresholds(ct);
}

static void ct406_als_mode_low_lux(struct ct406_data *ct)
{
	int error;
	u8 reg_data[2] = {0};

	/* clear AGL for regular ALS gain behavior */
	reg_data[0] = CT406_CONFIG;
	reg_data[1] = 0;
	error = ct406_i2c_write(ct, reg_data, 1);
	if (error < 0) {
		pr_err("%s: error writing CONFIG: %d\n", __func__, error);
		return;
	}

	/* write ALS gain = 8 */
	reg_data[0] = CT406_CONTROL;
	if (ct->hw_type == CT405_HW_TYPE)
		reg_data[1] = CT406_CONTROL_PDIODE_CH1
			| CT406_CONTROL_PGAIN_2X | CT406_CONTROL_AGAIN_8X;
	else
		reg_data[1] = CT406_CONTROL_PDIODE_CH1
			| CT406_CONTROL_PGAIN_1X | CT406_CONTROL_AGAIN_8X;
	error = ct406_i2c_write(ct, reg_data, 1);
	if (error < 0) {
		pr_err("%s: error writing ALS gain: %d\n", __func__, error);
		return;
	}

	/* write ALS integration time = ~49 ms */
	reg_data[0] = CT406_ATIME;
	reg_data[1] = CT406_ATIME_NOT_SATURATED;
	error = ct406_i2c_write(ct, reg_data, 1);
	if (error < 0) {
		pr_err("%s: error writing ATIME: %d\n", __func__, error);
		return;
	}

	ct->als_mode = CT406_ALS_MODE_LOW_LUX;
	ct->als_low_threshold = CT406_C0DATA_MAX - 1;
	ct406_write_als_thresholds(ct);
}

static void ct406_als_mode_high_lux(struct ct406_data *ct)
{
	int error;
	u8 reg_data[2] = {0};

	/* clear AGL for regular ALS gain behavior */
	reg_data[0] = CT406_CONFIG;
	reg_data[1] = 0;
	error = ct406_i2c_write(ct, reg_data, 1);
	if (error < 0) {
		pr_err("%s: error writing CONFIG: %d\n", __func__, error);
		return;
	}

	/* write ALS gain = 1 */
	reg_data[0] = CT406_CONTROL;
	if (ct->hw_type == CT405_HW_TYPE)
		reg_data[1] = CT406_CONTROL_PDIODE_CH1
			| CT406_CONTROL_PGAIN_2X | CT406_CONTROL_AGAIN_1X;
	else
		reg_data[1] = CT406_CONTROL_PDIODE_CH1
			| CT406_CONTROL_PGAIN_1X | CT406_CONTROL_AGAIN_1X;
	error = ct406_i2c_write(ct, reg_data, 1);
	if (error < 0) {
		pr_err("%s: error writing ALS gain: %d\n", __func__, error);
		return;
	}

	/* write ALS integration time = ~49 ms */
	reg_data[0] = CT406_ATIME;
	reg_data[1] = CT406_ATIME_NOT_SATURATED;
	error = ct406_i2c_write(ct, reg_data, 1);
	if (error < 0) {
		pr_err("%s: error writing ATIME: %d\n", __func__, error);
		return;
	}

	ct->als_mode = CT406_ALS_MODE_HIGH_LUX;
	ct->als_low_threshold = CT406_C0DATA_MAX - 1;
	ct406_write_als_thresholds(ct);
}

static void ct406_write_prox_thresholds(struct ct406_data *ct)
{
	u8 reg_data[5] = {0};
	unsigned int pilt = ct->prox_low_threshold;
	unsigned int piht = ct->prox_high_threshold;
	int error;

	reg_data[0] = (CT406_PILTL | CT406_COMMAND_AUTO_INCREMENT);
	reg_data[1] = (pilt & 0xFF);
	reg_data[2] = ((pilt >> 8) & 0xFF);
	reg_data[3] = (piht & 0xFF);
	reg_data[4] = ((piht >> 8) & 0xFF);

	error = ct406_i2c_write(ct, reg_data, 4);
	if (error < 0)
		pr_err("%s: Error writing new prox thresholds: %d\n",
			__func__, error);
}

static void ct406_prox_mode_saturated(struct ct406_data *ct)
{
	ct->prox_mode = CT406_PROX_MODE_SATURATED;
	pr_info("%s: Prox mode saturated\n", __func__);
}

static void ct406_prox_mode_uncovered(struct ct406_data *ct)
{
	unsigned int noise_floor = ct->prox_noise_floor;
	unsigned int pilt = noise_floor - ct->prox_recalibrate_offset;
	unsigned int piht = noise_floor + ct->prox_covered_offset;

	if (pilt > ct->pdata_max)
		pilt = 0;
	if (piht > ct->pdata_max)
		piht = ct->pdata_max;

	ct->prox_mode = CT406_PROX_MODE_UNCOVERED;
	ct->prox_low_threshold = pilt;
	ct->prox_high_threshold = piht;
	ct406_write_prox_thresholds(ct);
	pr_info("%s: Prox mode uncovered\n", __func__);
}

static void ct406_prox_mode_covered(struct ct406_data *ct)
{
	unsigned int noise_floor = ct->prox_noise_floor;
	unsigned int pilt = noise_floor + ct->prox_uncovered_offset;
	unsigned int piht = ct->pdata_max;

	if (pilt > ct->pdata_max)
		pilt = ct->pdata_max;

	ct->prox_mode = CT406_PROX_MODE_COVERED;
	ct->prox_low_threshold = pilt;
	ct->prox_high_threshold = piht;
	ct406_write_prox_thresholds(ct);
	pr_info("%s: Prox mode covered\n", __func__);
}

static void ct406_device_power_off(struct ct406_data *ct)
{
	int error;

	if (ct406_debug & CT406_DBG_POWER_ON_OFF)
		pr_info("%s: initialized=%d\n", __func__, ct->regs_initialized);

	if (ct->regs_initialized) {
		disable_irq_nosync(ct->client->irq);
		ct->oscillator_enabled = 0;
		error = ct406_write_enable(ct);
		if (error) {
			pr_err("%s: ct406_disable failed: %d\n",
				__func__, error);
		}
		ct->regs_initialized = 0;
	}

	if (ct->regulator) {
		error = regulator_disable(ct->regulator);
		if (error) {
			pr_err("%s: regulator_disable failed: %d\n",
				__func__, error);
		}
	}

}

static int ct406_device_power_on(struct ct406_data *ct)
{
	int error;

	if (ct406_debug & CT406_DBG_POWER_ON_OFF)
		pr_info("%s: initialized=%d\n", __func__, ct->regs_initialized);

	if (ct->regulator) {
		error = regulator_enable(ct->regulator);
		if (error) {
			pr_err("%s: regulator_enable failed: %d\n",
				__func__, error);
			return error;
		}
	}
	return 0;
}

static int ct406_device_init(struct ct406_data *ct)
{
	int error;

	if (!ct->regs_initialized) {
		error = ct406_init_registers(ct);
		if (error < 0) {
			pr_err("%s: init_registers failed: %d\n",
				__func__, error);
			if (ct->regulator)
				regulator_disable(ct->regulator);
			return error;
		}
		ct->oscillator_enabled = 0;
		ct->prox_enabled = 0;
		ct->prox_mode = CT406_PROX_MODE_SATURATED;
		ct->als_enabled = 0;
		ct->als_mode = CT406_ALS_MODE_LOW_LUX;

		enable_irq(ct->client->irq);

		ct->regs_initialized = 1;
	}

	return 0;
}

static void ct406_check_als_range(struct ct406_data *ct, unsigned int lux)
{
	if (ct->als_mode == CT406_ALS_MODE_LOW_LUX) {
		if (lux >= CT406_ALS_LOW_TO_HIGH_THRESHOLD)
			ct406_als_mode_high_lux(ct);
	} else if (ct->als_mode == CT406_ALS_MODE_HIGH_LUX) {
		if (lux < CT406_ALS_HIGH_TO_LOW_THRESHOLD)
			ct406_als_mode_low_lux(ct);
	}
}

static int ct406_enable_als(struct ct406_data *ct)
{
	int error;
	u8 reg_data[5] = {0};

	if (ct->ip_enabled)
		return 0;

	if (ct->suspended)
		return 0;

	if (ct->als_mode != CT406_ALS_MODE_SUNLIGHT) {
		error = ct406_set_als_enable(ct, 0);
		if (error) {
			pr_err("%s: Unable to turn off ALS: %d\n",
				__func__, error);
			return error;
		}

		ct406_als_mode_low_lux(ct);

		/* write wait time = ALS on value */
		reg_data[0] = CT406_WTIME;
		reg_data[1] = CT406_WTIME_ALS_ON;
		error = ct406_i2c_write(ct, reg_data, 1);
		if (error < 0) {
			pr_err("%s: Error  %d\n", __func__, error);
			return error;
		}
		error = ct406_set_wait_enable(ct, 0);
		if (error) {
			pr_err("%s: Unable to set wait enable: %d\n",
				__func__, error);
			return error;
		}

		/* write ALS interrupt persistence */
		reg_data[0] = CT406_PERS;
		reg_data[1] = CT406_PERS_PPERS;
		error = ct406_i2c_write(ct, reg_data, 1);
		if (error < 0) {
			pr_err("%s: Error  %d\n", __func__, error);
			return error;
		}
		ct->als_first_report = 0;

		ct406_clear_als_flag(ct);

		error = ct406_set_als_enable(ct, 1);
		if (error) {
			pr_err("%s: Unable to turn on ALS: %d\n",
				__func__, error);
			return error;
		}
	}

	return 0;
}

static int ct406_disable_als(struct ct406_data *ct)
{
	int error;
	u8 reg_data[2] = {0};

	if (ct->als_enabled && ct->als_mode != CT406_ALS_MODE_SUNLIGHT) {
		ct406_set_als_enable(ct, 0);

		/* write wait time = ALS off value */
		reg_data[0] = CT406_WTIME;
		reg_data[1] = CT406_WTIME_ALS_OFF;
		error = ct406_i2c_write(ct, reg_data, 1);
		if (error < 0) {
			pr_err("%s: Error  %d\n", __func__, error);
			return error;
		}
		error = ct406_set_wait_enable(ct, 1);
		if (error) {
			pr_err("%s: Unable to set wait enable: %d\n",
				__func__, error);
			return error;
		}

		ct406_clear_als_flag(ct);
	}

	return 0;
}

static void ct406_measure_noise_floor(struct ct406_data *ct)
{
	int error = -EINVAL;
	unsigned int num_samples = ct->pdata->prox_samples_for_noise_floor;
	unsigned int i, sum = 0, avg = 0;
	unsigned int max = ct->pdata_max - 1 - ct->prox_covered_offset;
	u8 reg_data[2] = {0};

	/* disable ALS temporarily */
	error = ct406_set_als_enable(ct, 0);
	if (error < 0)
		pr_err("%s: error disabling ALS: %d\n",
			__func__, error);

	/* clear AGL for regular ALS gain behavior */
	reg_data[0] = CT406_CONFIG;
	reg_data[1] = 0;
	error = ct406_i2c_write(ct, reg_data, 1);
	if (error < 0)
		pr_err("%s: error writing CONFIG: %d\n",
			__func__, error);

	/* disable wait */
	error = ct406_set_wait_enable(ct, 0);
	if (error) {
		pr_err("%s: Unable to set wait enable: %d\n",
			__func__, error);
	}

	for (i = 0; i < num_samples; i++) {
		/* enable prox sensor and wait */
		error = ct406_set_prox_enable(ct, 1);
		if (error) {
			pr_err("%s: Error enabling proximity sensor: %d\n",
				__func__, error);
			break;
		}
		usleep_range(7000, 7100);

		reg_data[0] = (CT406_PDATA | CT406_COMMAND_AUTO_INCREMENT);
		error = ct406_i2c_read(ct, reg_data, 2);
		if (error) {
			pr_err("%s: Error reading prox data: %d\n",
				__func__, error);
			break;
		}
		sum += (reg_data[1] << 8) | reg_data[0];

		/* disable prox sensor */
		error = ct406_set_prox_enable(ct, 0);
		if (error) {
			pr_err("%s: Error disabling proximity sensor: %d\n",
				__func__, error);
			break;
		}
	}

	if (!error)
		avg = sum / num_samples;

	if (avg < max)
		ct->prox_noise_floor = avg;
	else
		ct->prox_noise_floor = max;

	if (ct406_debug & CT406_DBG_ENABLE_DISABLE)
		pr_info("%s: Noise floor is 0x%x\n", __func__,
			ct->prox_noise_floor);

	if (ct->prox_noise_floor < ct->prox_saturation_threshold) {
		input_event(ct->dev, EV_MSC, MSC_RAW,
			CT406_PROXIMITY_FAR);
		input_sync(ct->dev);
		ct406_prox_mode_uncovered(ct);
	} else {
		input_event(ct->dev, EV_MSC, MSC_RAW,
			CT406_PROXIMITY_NEAR);
		input_sync(ct->dev);
		ct406_prox_mode_covered(ct);
	}

	error = ct406_set_prox_enable(ct, 1);
	if (error)
		pr_err("%s: Error enabling proximity sensor: %d\n",
			__func__, error);

	/* re-enable ALS if necessary */
	ct406_als_mode_low_lux(ct);
	if (ct->als_requested && !ct->suspended)
		ct406_enable_als(ct);
}

static int ct406_check_saturation(struct ct406_data *ct)
{
	int error = 0;
	u8 reg_data[2] = {0};
	unsigned int c0data;

	/* disable prox */
	ct406_set_prox_enable(ct, 0);
	ct->prox_low_threshold = 0;
	ct->prox_high_threshold = ct->pdata_max;
	ct406_write_prox_thresholds(ct);

	ct406_als_mode_sunlight(ct);
	ct406_set_als_enable(ct, 1);

	/* write ALS interrupt persistence = saturated value */
	reg_data[0] = CT406_PERS;
	reg_data[1] = CT406_PERS_PPERS | CT406_PERS_APERS_SATURATED;
	error = ct406_i2c_write(ct, reg_data, 1);
	if (error < 0)
		return 0;

	/* write wait time = saturated value */
	reg_data[0] = CT406_WTIME;
	reg_data[1] = CT406_WTIME_SATURATED;
	error = ct406_i2c_write(ct, reg_data, 1);
	if (error < 0)
		return 0;
	error = ct406_set_wait_enable(ct, 1);
	if (error) {
		pr_err("%s: Unable to set wait enable: %d\n",
			__func__, error);
		return error;
	}

	usleep_range(7000, 7100);

	/* read C0DATA */
	reg_data[0] = (CT406_C0DATA | CT406_COMMAND_AUTO_INCREMENT);
	error = ct406_i2c_read(ct, reg_data, 2);
	if (error < 0)
		return 0;
	c0data = (reg_data[1] << 8) | reg_data[0];

	return (c0data > ct->als_low_threshold);
}

static int ct406_enable_prox(struct ct406_data *ct)
{
	int error;

	if (ct->ip_enabled)
		return 0;

	if (ct->suspended) {
		if (ct406_debug & CT406_DBG_ENABLE_DISABLE)
			pr_info("%s: Powering on\n", __func__);
		error = ct406_device_power_on(ct);
		if (error)
			return error;
		error = ct406_device_init(ct);
		if (error)
			return error;
	}

	if (ct406_check_saturation(ct))
		ct406_prox_mode_saturated(ct);
	else
		ct406_measure_noise_floor(ct);

	return 0;
}

static int ct406_disable_prox(struct ct406_data *ct)
{
	if (ct->prox_enabled) {
		ct406_set_prox_enable(ct, 0);
		ct406_clear_prox_flag(ct);
		ct406_prox_mode_saturated(ct);

		if (ct->als_requested) {
			if (ct->als_mode == CT406_ALS_MODE_SUNLIGHT) {
				ct406_als_mode_low_lux(ct);
				if (!ct->suspended)
					ct406_enable_als(ct);
			}
		}
	}

	if (ct->suspended && ct->regs_initialized && !ct->ip_enabled) {
		if (ct406_debug & CT406_DBG_ENABLE_DISABLE)
			pr_info("%s: Powering off\n", __func__);
		ct406_device_power_off(ct);
	}

	return 0;
}

static void ct406_start_ip(struct ct406_data *ct)
{
	int error;
	u8 reg_data[2] = {0x00, 0x00};

	if (ct->suspended) {
		if (ct406_debug & CT406_DBG_ENABLE_DISABLE)
			pr_info("%s: Powering on\n", __func__);
		error = ct406_device_power_on(ct);
		if (error)
			return;
		error = ct406_device_init(ct);
		if (error)
			return;
	}

	ct406_als_mode_low_lux(ct);

	reg_data[0] = CT406_PERS;
	reg_data[1] = CT406_PERS_PPERS;
	error = ct406_i2c_write(ct, reg_data, 1);
	if (error < 0) {
		pr_err("%s: Error  %d\n", __func__, error);
		return;
	}

	if (ct406_debug & CT406_DBG_INPUT)
		pr_info("%s: Enable sensors\n", __func__);
	reg_data[0] = CT406_ENABLE;
	reg_data[1] = CT406_ENABLE_PON;
	if (!ct->oscillator_enabled) {
		error = ct406_i2c_write(ct, reg_data, 1);
		if (error < 0) {
			pr_err("%s: Error  %d\n", __func__, error);
			return;
		}
		usleep_range(3000, 3100);
		ct->oscillator_enabled = 1;
	}
	reg_data[1] |= CT406_ENABLE_PEN | CT406_ENABLE_AEN | CT406_ENABLE_AIEN;
	error = ct406_i2c_write(ct, reg_data, 1);
	if (error < 0)
		pr_err("%s: Error  %d\n", __func__, error);
}


static void ct406_stop_ip(struct ct406_data *ct)
{
	u8 reg_data[4] = {0x00, 0x00, 0x00, 0x00};
	reg_data[0] = CT406_ENABLE;
	reg_data[1] = CT406_ENABLE_PON;
	ct406_i2c_write(ct, reg_data, 1);
}

static int ct406_enable_ip(struct ct406_data *ct)
{
	if (ct->prox_enabled)
		ct406_disable_prox(ct);

	if (ct->als_enabled)
		ct406_disable_als(ct);

	if (ct406_debug & CT406_DBG_ENABLE_DISABLE)
		pr_info("%s: Enable In Pocket\n", __func__);

	ct->ip_enabled = 1;
	ct406_start_ip(ct);

	return 0;
}

static int ct406_disable_ip(struct ct406_data *ct)
{
	if (ct406_debug & CT406_DBG_ENABLE_DISABLE)
		pr_info("%s: Disable In Pocket\n", __func__);

	ct406_stop_ip(ct);
	ct->ip_enabled = 0;

	if (ct->prox_requested)
		ct406_enable_prox(ct);

	if (ct->als_requested)
		ct406_enable_als(ct);

	return 0;
}

static void ct406_report_prox(struct ct406_data *ct)
{
	int error = 0;
	u8 reg_data[2] = {0};
	unsigned int pdata = 0;

	reg_data[0] = (CT406_PDATA | CT406_COMMAND_AUTO_INCREMENT);
	error = ct406_i2c_read(ct, reg_data, 2);
	if (error < 0)
		return;

	pdata = (reg_data[1] << 8) | reg_data[0];
	if (ct406_debug & CT406_DBG_INPUT)
		pr_info("%s: PDATA = %d\n", __func__, pdata);

	switch (ct->prox_mode) {
	case CT406_PROX_MODE_SATURATED:
		pr_warn("%s: prox interrupted in saturated mode!\n", __func__);
		break;
	case CT406_PROX_MODE_UNCOVERED:
		if (pdata < ct->prox_low_threshold)
			ct406_enable_prox(ct);
		if (pdata > ct->prox_high_threshold) {
			input_event(ct->dev, EV_MSC, MSC_RAW,
				CT406_PROXIMITY_NEAR);
			input_sync(ct->dev);
			ct406_prox_mode_covered(ct);
		}
		break;
	case CT406_PROX_MODE_COVERED:
		if (pdata < ct->prox_low_threshold) {
			input_event(ct->dev, EV_MSC, MSC_RAW,
				CT406_PROXIMITY_FAR);
			input_sync(ct->dev);
			ct406_prox_mode_uncovered(ct);
		}
		break;
	default:
		pr_err("%s: prox mode is %d!\n", __func__, ct->prox_mode);
	}

	ct406_clear_prox_flag(ct);
}

static void ct406_report_als(struct ct406_data *ct)
{
	int error;
	u8 reg_data[4] = {0};
	unsigned int c0data;
	unsigned int c1data;
	unsigned int ratio;
	unsigned int lux = 0;

	reg_data[0] = (CT406_C0DATA | CT406_COMMAND_AUTO_INCREMENT);
	error = ct406_i2c_read(ct, reg_data, 4);
	if (error < 0)
		return;

	c0data = (reg_data[1] << 8) | reg_data[0];
	c1data = (reg_data[3] << 8) | reg_data[2];
	if (ct406_debug & CT406_DBG_INPUT)
		pr_info("%s: C0DATA = %d, C1DATA = %d\n",
			 __func__, c0data, c1data);

	/* calculate lux using piecewise function from TAOS */
	if (c0data == 0)
		c0data = 1;
	ratio = ((100 * c1data) + c0data - 1) / c0data;
	switch (ct->als_mode) {
	case CT406_ALS_MODE_SUNLIGHT:
		if (c0data == 0x0400 || c1data == 0x0400)
			lux = 0xFFFF;
		else if (ratio <= 51)
			lux = (1041*c0data - 1963*c1data);
		else if (ratio <= 58)
			lux = (342*c0data - 587*c1data);
		else
			lux = 0;
		break;
	case CT406_ALS_MODE_LOW_LUX:
		if (c0data == 0x4800 || c1data == 0x4800)
			lux = CT406_ALS_LOW_TO_HIGH_THRESHOLD;
		else if (ratio <= 51)
			lux = (121*c0data - 227*c1data) / 100;
		else if (ratio <= 58)
			lux = (40*c0data - 68*c1data) / 100;
		else
			lux = 0;
		break;
	case CT406_ALS_MODE_HIGH_LUX:
		if (c0data == 0x4800 || c1data == 0x4800)
			lux = 0xFFFF;
		else if (ratio <= 51)
			lux = (964*c0data - 1818*c1data) / 100;
		else if (ratio <= 58)
			lux = (317*c0data - 544*c1data) / 100;
		else
			lux = 0;
		break;
	default:
		pr_err("%s: ALS mode is %d!\n", __func__, ct->als_mode);
	}

	/* input.c filters consecutive LED_MISC values <=1. */
	lux = (lux >= 2) ? lux : 2;

	input_event(ct->dev, EV_LED, LED_MISC, lux);
	input_sync(ct->dev);

	if (ct->als_first_report == 0) {
		/* write ALS interrupt persistence */
		reg_data[0] = CT406_PERS;
		reg_data[1] = CT406_PERS_PPERS | ct->als_apers;
		error = ct406_i2c_write(ct, reg_data, 1);
		if (error < 0) {
			pr_err("%s: Error  %d\n", __func__, error);
		}
		ct->als_first_report = 1;
	}

	if (ct->als_mode != CT406_ALS_MODE_SUNLIGHT)
		ct406_check_als_range(ct, lux);

	ct406_clear_als_flag(ct);

	if (ct->als_mode == CT406_ALS_MODE_SUNLIGHT)
		ct406_enable_prox(ct); /* re-check saturation */
}

static void ct406_report_ip(struct ct406_data *ct)
{
	int error;
	u8 reg_data[4] = {0x00, 0x00, 0x00, 0x00};
	unsigned int pdata;
	unsigned int c0data;
	unsigned int c1data;
	unsigned int ratio;
	unsigned int lux = 0;

	reg_data[0] = (CT406_PDATA | CT406_COMMAND_AUTO_INCREMENT);
	error = ct406_i2c_read(ct, reg_data, 2);
	if (error) {
		pr_err("%s: Error reading prox data: %d\n",
			__func__, error);
		return;
	}

	pdata = (reg_data[1] << 8) | reg_data[0];

	reg_data[0] = (CT406_C0DATA | CT406_COMMAND_AUTO_INCREMENT);
	error = ct406_i2c_read(ct, reg_data, 4);
	if (error < 0) {
		pr_err("%s: Error reading als data: %d\n",
			__func__, error);
		return;
	}

	c0data = (reg_data[1] << 8) | reg_data[0];
	c1data = (reg_data[3] << 8) | reg_data[2];
	if (ct406_debug & CT406_DBG_INPUT)
		pr_info("%s: C0DATA = %d, C1DATA = %d\n",
			__func__, c0data, c1data);

	if (c0data == 0)
		c0data = 1;
	ratio = ((100 * c1data) + c0data - 1) / c0data;
	if (c0data == 0x4800 || c1data == 0x4800)
		lux = CT406_ALS_LOW_TO_HIGH_THRESHOLD;
	else if (ratio <= 51)
		lux = (121*c0data - 227*c1data) / 100;
	else if (ratio <= 58)
		lux = (40*c0data - 68*c1data) / 100;
	else
		lux = 0;

	if (ct406_debug & CT406_DBG_INPUT) {
		pr_info("%s: PDATA = %d\n", __func__, pdata);
		pr_info("%s: Lux = %d\n", __func__, lux);
	}

	if ((pdata > ct->pdata->ip_prox_limit)
		&& (lux < ct->pdata->ip_als_limit)) {
		input_event(ct->dev, EV_MSC, MSC_PULSELED, 1);
		input_sync(ct->dev);
	} else {
		input_event(ct->dev, EV_MSC, MSC_PULSELED, 0);
		input_sync(ct->dev);
	}

	ct406_clear_prox_flag(ct);
	ct406_clear_als_flag(ct);
}

static int ct406_misc_open(struct inode *inode, struct file *file)
{
	int err;

	err = nonseekable_open(inode, file);
	if (err < 0)
		return err;

	file->private_data = ct406_misc_data;

	return 0;
}

static int ct406_als_enable_param;

static int ct406_set_als_enable_param(const char *char_value,
	struct kernel_param *kp)
{
	unsigned long int_value;
	int  ret;

	if (!char_value)
		return -EINVAL;

	if (!ct406_misc_data)
		return -EINVAL;

	ret = strict_strtoul(char_value, (unsigned int)0, &int_value);
	if (ret)
		return ret;

	*((int *)kp->arg) = int_value;

	mutex_lock(&ct406_misc_data->mutex);

	ct406_misc_data->als_requested = (int_value != 0);
	if (ct406_debug & CT406_DBG_INPUT)
		pr_info("%s: als enable = %d\n", __func__,
			ct406_misc_data->als_requested);
	if (ct406_misc_data->als_requested)
		ret = ct406_enable_als(ct406_misc_data);
	else
		ret = ct406_disable_als(ct406_misc_data);

	mutex_unlock(&ct406_misc_data->mutex);

	return ret;
}

static int ct406_get_als_enable_param(char *buffer, struct kernel_param *kp)
{
	int num_chars;

	if (!buffer)
		return -EINVAL;

	if (!ct406_misc_data) {
		scnprintf(buffer, PAGE_SIZE, "0\n");
		return 1;
	}

	num_chars = scnprintf(buffer, 2, "%d\n",
		ct406_misc_data->als_requested);

	return num_chars;
}

module_param_call(als_enable, ct406_set_als_enable_param,
	ct406_get_als_enable_param, &ct406_als_enable_param, 0644);
MODULE_PARM_DESC(als_enable, "Enable/disable the ALS.");

static int ct406_prox_enable_param;

static int ct406_set_prox_enable_param(const char *char_value,
	struct kernel_param *kp)
{
	unsigned long int_value;
	int  ret;

	if (!char_value)
		return -EINVAL;

	if (!ct406_misc_data)
		return -EINVAL;

	ret = strict_strtoul(char_value, (unsigned int)0, &int_value);
	if (ret)
		return ret;

	*((int *)kp->arg) = int_value;

	mutex_lock(&ct406_misc_data->mutex);

	ct406_misc_data->prox_requested = (int_value != 0);
	if (ct406_debug & CT406_DBG_INPUT)
		pr_info("%s: prox enable = %d\n", __func__,
			ct406_misc_data->prox_requested);
	if (ct406_misc_data->prox_requested)
		ret = ct406_enable_prox(ct406_misc_data);
	else
		ret = ct406_disable_prox(ct406_misc_data);

	mutex_unlock(&ct406_misc_data->mutex);

	return ret;
}

static int ct406_get_prox_enable_param(char *buffer, struct kernel_param *kp)
{
	int num_chars;

	if (!buffer)
		return -EINVAL;

	if (!ct406_misc_data) {
		scnprintf(buffer, PAGE_SIZE, "0\n");
		return 1;
	}

	num_chars = scnprintf(buffer, 2, "%d\n",
		ct406_misc_data->prox_requested);

	return num_chars;
}

module_param_call(prox_enable, ct406_set_prox_enable_param,
	ct406_get_prox_enable_param, &ct406_prox_enable_param, 0644);
MODULE_PARM_DESC(prox_enable, "Enable/disable the Prox.");

static int ct406_ip_enable_param;

static int ct406_set_ip_enable_param(const char *char_value,
	struct kernel_param *kp)
{
	unsigned long int_value;
	int  ret;

	if (!char_value)
		return -EINVAL;

	if (!ct406_misc_data)
		return -EINVAL;

	ret = strict_strtoul(char_value, (unsigned int)0, &int_value);
	if (ret)
		return ret;

	*((int *)kp->arg) = int_value;

	mutex_lock(&ct406_misc_data->mutex);

	ct406_misc_data->ip_requested = (int_value != 0);
	if (ct406_debug & CT406_DBG_INPUT)
		pr_info("%s: ip enable = %d\n", __func__,
			ct406_misc_data->ip_requested);
	if (ct406_misc_data->ip_requested)
		ret = ct406_enable_ip(ct406_misc_data);
	else
		ret = ct406_disable_ip(ct406_misc_data);

	mutex_unlock(&ct406_misc_data->mutex);

	return ret;
}

static int ct406_get_ip_enable_param(char *buffer, struct kernel_param *kp)
{
	int num_chars;

	if (!buffer)
		return -EINVAL;

	if (!ct406_misc_data) {
		scnprintf(buffer, PAGE_SIZE, "0\n");
		return 1;
	}

	num_chars = scnprintf(buffer, 2, "%d\n",
		ct406_misc_data->ip_requested);

	return num_chars;
}

module_param_call(ip_enable, ct406_set_ip_enable_param,
	ct406_get_ip_enable_param, &ct406_ip_enable_param, 0644);
MODULE_PARM_DESC(ip_enable, "Enable/disable In Pocket");

static int ct406_als_delay_param;

static int ct406_set_als_delay_param(const char *char_value,
	struct kernel_param *kp)
{
	unsigned long int_value;
	int  ret;

	if (!char_value)
		return -EINVAL;

	if (!ct406_misc_data)
		return -EINVAL;

	ret = strict_strtoul(char_value, (unsigned int)0, &int_value);
	if (ret)
		return ret;

	*((int *)kp->arg) = int_value;

	mutex_lock(&ct406_misc_data->mutex);

	ct406_als_delay_param = int_value;
	if (ct406_debug & CT406_DBG_INPUT)
		pr_info("%s: delay = %d\n", __func__, ct406_als_delay_param);

	if (ct406_als_delay_param > 982)
		ct406_misc_data->als_apers = 0x07;
	else if (ct406_als_delay_param > 737)
		ct406_misc_data->als_apers = 0x06;
	else if (ct406_als_delay_param > 491)
		ct406_misc_data->als_apers = 0x05;
	else if (ct406_als_delay_param > 245)
		ct406_misc_data->als_apers = 0x04;
	else if (ct406_als_delay_param > 147)
		ct406_misc_data->als_apers = 0x03;
	else if (ct406_als_delay_param > 98)
		ct406_misc_data->als_apers = 0x02;
	else
		ct406_misc_data->als_apers = 0x01;

	if (ct406_misc_data->als_enabled
		&& (ct406_misc_data->als_mode != CT406_ALS_MODE_SUNLIGHT)) {
		ct406_disable_als(ct406_misc_data);
		ct406_enable_als(ct406_misc_data);
	}

	mutex_unlock(&ct406_misc_data->mutex);

	return ret;
}

static int ct406_get_als_delay_param(char *buffer, struct kernel_param *kp)
{
	int num_chars;

	if (!buffer)
		return -EINVAL;

	if (!ct406_misc_data) {
		scnprintf(buffer, PAGE_SIZE, "0\n");
		return 1;
	}

	num_chars = scnprintf(buffer, 2, "%d\n", ct406_als_delay_param);

	return num_chars;
}

module_param_call(als_delay, ct406_set_als_delay_param, ct406_get_als_delay_param,
	&ct406_als_delay_param, 0644);
MODULE_PARM_DESC(als_delay, "Set ALS delay.");

static const struct file_operations ct406_misc_fops = {
	.owner = THIS_MODULE,
	.open = ct406_misc_open,
};

static ssize_t ct406_registers_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct ct406_data *ct = i2c_get_clientdata(client);
	int error = 0;
	unsigned int i, n, reg_count;
	u8 reg_data[1] = {0};

	reg_count = sizeof(ct406_regs) / sizeof(ct406_regs[0]);
	mutex_lock(&ct->mutex);
	for (i = 0, n = 0; i < reg_count; i++) {
		reg_data[0] = ct406_regs[i].reg;
		error = ct406_i2c_read(ct, reg_data, 1);
		n += scnprintf(buf + n, PAGE_SIZE - n,
			"%-20s = 0x%02X\n",
			ct406_regs[i].name,
			reg_data[0]);
	}
	mutex_unlock(&ct->mutex);

	return n;
}

static ssize_t ct406_registers_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf,
				     size_t count)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct ct406_data *ct = i2c_get_clientdata(client);
	unsigned int i, reg_count;
	unsigned int value;
	u8 reg_data[2] = {0};
	int error;
	char name[30];

	if (count >= 30) {
		pr_err("%s:input too long\n", __func__);
		return -EMSGSIZE;
	}

	if (sscanf(buf, "%30s %x", name, &value) != 2) {
		pr_err("%s:unable to parse input\n", __func__);
		return -EINVAL;
	}

	reg_count = sizeof(ct406_regs) / sizeof(ct406_regs[0]);
	for (i = 0; i < reg_count; i++) {
		if (!strcmp(name, ct406_regs[i].name)) {
			mutex_lock(&ct->mutex);
			error = ct406_i2c_write(ct, reg_data, 1);
			mutex_unlock(&ct->mutex);
			if (error) {
				pr_err("%s:Failed to write register %s\n",
					__func__, name);
				return error;
			}
			return count;
		}
	}

	pr_err("%s:no such register %s\n", __func__, name);
	return -EINVAL;
}

static DEVICE_ATTR(registers, 0644, ct406_registers_show,
		   ct406_registers_store);

static irqreturn_t ct406_irq_handler(int irq, void *dev)
{
	struct ct406_data *ct = dev;

	disable_irq_nosync(ct->client->irq);
	queue_work(ct->workqueue, &ct->work);

	return IRQ_HANDLED;
}

static void ct406_work_func_locked(struct ct406_data *ct)
{
	int error;
	u8 reg_data[1] = {0};
	reg_data[0] = CT406_STATUS;
	error = ct406_i2c_read(ct, reg_data, 1);
	if (error < 0) {
		pr_err("%s: Unable to read interrupt register: %d\n",
			__func__, error);
		return;
	}

	if (ct->ip_enabled) {
		ct406_report_ip(ct);
		return;
	}

	if (ct->als_enabled && (reg_data[0] & CT406_STATUS_AINT))
		ct406_report_als(ct);

	if (ct->prox_enabled && (reg_data[0] & CT406_STATUS_PINT))
		ct406_report_prox(ct);

	if (ct->suspended)
		ct406_disable_als(ct);
}

static void ct406_work_func(struct work_struct *work)
{
	struct ct406_data *ct =
		container_of(work, struct ct406_data, work);

	mutex_lock(&ct->mutex);
	if (ct->regs_initialized)
		ct406_work_func_locked(ct);
	mutex_unlock(&ct->mutex);

	enable_irq(ct->client->irq);
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ct406_suspend(struct early_suspend *handler)
{
	if (ct406_debug & CT406_DBG_SUSPEND_RESUME)
		pr_info("%s\n", __func__);

	mutex_lock(&ct406_misc_data->mutex);

	ct406_disable_als(ct406_misc_data);

	if (!ct406_misc_data->prox_requested)
		ct406_device_power_off(ct406_misc_data);

	ct406_misc_data->suspended = 1;

	mutex_unlock(&ct406_misc_data->mutex);
}

static void ct406_resume(struct early_suspend *handler)
{
	if (ct406_debug & CT406_DBG_SUSPEND_RESUME)
		pr_info("%s\n", __func__);

	mutex_lock(&ct406_misc_data->mutex);

	ct406_device_power_on(ct406_misc_data);
	ct406_device_init(ct406_misc_data);

	ct406_misc_data->suspended = 0;

	if (ct406_misc_data->als_requested)
		ct406_enable_als(ct406_misc_data);

	mutex_unlock(&ct406_misc_data->mutex);
}
#endif /* CONFIG_HAS_EARLYSUSPEND */

static int ct406_probe(struct i2c_client *client,
		       const struct i2c_device_id *id)
{
	struct ct406_platform_data *pdata = client->dev.platform_data;
	struct ct406_data *ct;
	u8 reg_data[1] = {0};
	int error = 0;

	if (pdata == NULL) {
		pr_err("%s: platform data required\n", __func__);
		return -ENODEV;
	}

	client->irq = pdata->irq;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s:I2C_FUNC_I2C not supported\n", __func__);
		return -ENODEV;
	}

	ct = kzalloc(sizeof(struct ct406_data), GFP_KERNEL);
	if (ct == NULL) {
		error = -ENOMEM;
		goto error_alloc_data_failed;
	}

	ct->client = client;
	ct->pdata = pdata;

	if (ct->pdata->regulator_name[0] != '\0') {
		ct->regulator = regulator_get(NULL,
			ct->pdata->regulator_name);
		if (IS_ERR(ct->regulator)) {
			pr_err("%s: cannot acquire regulator [%s]\n",
				__func__, ct->pdata->regulator_name);
			error = PTR_ERR(ct->regulator);
			goto error_regulator_get_failed;
		}
	}

	ct->dev = input_allocate_device();
	if (!ct->dev) {
		error = -ENOMEM;
		pr_err("%s: input device allocate failed: %d\n", __func__,
			error);
		goto error_input_allocate_failed;
	}

	ct->dev->name = "light-prox";
	input_set_capability(ct->dev, EV_LED, LED_MISC);
	input_set_capability(ct->dev, EV_MSC, MSC_RAW);
	input_set_capability(ct->dev, EV_MSC, MSC_PULSELED);

	ct406_misc_data = ct;
	ct->miscdevice.minor = MISC_DYNAMIC_MINOR;
	ct->miscdevice.name = LD_CT406_NAME;
	ct->miscdevice.fops = &ct406_misc_fops;
	error = misc_register(&ct->miscdevice);
	if (error < 0) {
		pr_err("%s: misc_register failed\n", __func__);
		goto error_misc_register_failed;
	}

	ct->regs_initialized = 0;
	ct->suspended = 0;

	ct->oscillator_enabled = 0;
	ct->prox_requested = 0;
	ct->prox_enabled = 0;
	ct->prox_mode = CT406_PROX_MODE_SATURATED;
	ct->als_requested = 0;
	ct->als_enabled = 0;
	ct->als_apers = 0x4;
	ct->als_first_report = 0;
	ct->als_mode = CT406_ALS_MODE_LOW_LUX;
	ct->ip_requested = 0;
	ct->ip_enabled = 0;

	ct->workqueue = create_singlethread_workqueue("als_wq");
	if (!ct->workqueue) {
		pr_err("%s: Cannot create work queue\n", __func__);
		error = -ENOMEM;
		goto error_create_wq_failed;
	}

	INIT_WORK(&ct->work, ct406_work_func);

	error = request_irq(client->irq, ct406_irq_handler,
		IRQF_TRIGGER_FALLING, LD_CT406_NAME, ct);
	if (error != 0) {
		pr_err("%s: irq request failed: %d\n", __func__, error);
		error = -ENODEV;
		goto error_req_irq_failed;
	}
	enable_irq_wake(client->irq);
	disable_irq(client->irq);

	i2c_set_clientdata(client, ct);

	mutex_init(&ct->mutex);

	error = input_register_device(ct->dev);
	if (error) {
		pr_err("%s: input device register failed:%d\n", __func__,
			error);
		goto error_input_register_failed;
	}

	error = device_create_file(&client->dev, &dev_attr_registers);
	if (error < 0) {
		pr_err("%s:File device creation failed: %d\n", __func__, error);
		error = -ENODEV;
		goto error_create_registers_file_failed;
	}

	error = ct406_device_power_on(ct);
	if (error) {
		pr_err("%s:power_on failed: %d\n", __func__, error);
		goto error_power_on_failed;
	}

	reg_data[0] = CT406_REV_ID;
	error = ct406_i2c_read(ct, reg_data, 1);
	if (error < 0) {
		pr_err("%s: revision read failed: %d\n", __func__, error);
		goto error_revision_read_failed;
	}
	if (reg_data[0] == CT40X_REV_ID_CT405) {
		pr_info("%s: CT405 part detected\n", __func__);
		ct->prox_saturation_threshold
			= ct->pdata->ct405_prox_saturation_threshold;
		ct->prox_covered_offset = ct->pdata->ct405_prox_covered_offset;
		ct->prox_uncovered_offset
			= ct->pdata->ct405_prox_uncovered_offset;
		ct->prox_recalibrate_offset
			= ct->pdata->ct405_prox_recalibrate_offset;
		ct->prox_pulse_count = 2;
		ct->pdata_max = CT405_PDATA_MAX;
		ct->hw_type = CT405_HW_TYPE;
	} else {
		pr_info("%s: CT406 part detected\n", __func__);
		ct->prox_saturation_threshold
			= ct->pdata->ct406_prox_saturation_threshold;
		ct->prox_covered_offset = ct->pdata->ct406_prox_covered_offset;
		ct->prox_uncovered_offset
			= ct->pdata->ct406_prox_uncovered_offset;
		ct->prox_recalibrate_offset
			= ct->pdata->ct406_prox_recalibrate_offset;
		ct->prox_pulse_count = ct->pdata->ct406_prox_pulse_count;
		ct->prox_offset = ct->pdata->ct406_prox_offset;
		ct->pdata_max = CT406_PDATA_MAX;
		ct->hw_type = CT406_HW_TYPE;
	}

	error = ct406_device_init(ct);
	if (error) {
		pr_err("%s:device init failed: %d\n", __func__, error);
		goto error_revision_read_failed;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	ct->ct406_early_suspend.suspend = ct406_suspend;
	ct->ct406_early_suspend.resume = ct406_resume;
	register_early_suspend(&ct->ct406_early_suspend);
#endif

	return 0;

error_revision_read_failed:
	ct406_device_power_off(ct);
error_power_on_failed:
	device_remove_file(&client->dev, &dev_attr_registers);
error_create_registers_file_failed:
	input_unregister_device(ct->dev);
	ct->dev = NULL;
error_input_register_failed:
	mutex_destroy(&ct->mutex);
	i2c_set_clientdata(client, NULL);
	free_irq(ct->client->irq, ct);
error_req_irq_failed:
	destroy_workqueue(ct->workqueue);
error_create_wq_failed:
	misc_deregister(&ct->miscdevice);
error_misc_register_failed:
	ct406_misc_data = NULL;
	input_free_device(ct->dev);
error_input_allocate_failed:
	regulator_put(ct->regulator);
error_regulator_get_failed:
	kfree(ct);
error_alloc_data_failed:
	return error;
}

static int ct406_remove(struct i2c_client *client)
{
	struct ct406_data *ct = i2c_get_clientdata(client);

	device_remove_file(&client->dev, &dev_attr_registers);

	ct406_device_power_off(ct);

	input_unregister_device(ct->dev);

	mutex_destroy(&ct->mutex);
	i2c_set_clientdata(client, NULL);
	free_irq(ct->client->irq, ct);

	destroy_workqueue(ct->workqueue);

	misc_deregister(&ct->miscdevice);

	regulator_put(ct->regulator);

	kfree(ct);

	return 0;
}

static const struct i2c_device_id ct406_id[] = {
	{LD_CT406_NAME, 0},
	{}
};

static struct i2c_driver ct406_i2c_driver = {
	.probe		= ct406_probe,
	.remove		= ct406_remove,
	.id_table	= ct406_id,
	.driver = {
		.name = LD_CT406_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init ct406_init(void)
{
	return i2c_add_driver(&ct406_i2c_driver);
}

static void __exit ct406_exit(void)
{
	i2c_del_driver(&ct406_i2c_driver);
}

module_init(ct406_init);
module_exit(ct406_exit);

MODULE_DESCRIPTION("ALS and Proximity driver for CT406");
MODULE_AUTHOR("Motorola Mobility");
MODULE_LICENSE("GPL");
