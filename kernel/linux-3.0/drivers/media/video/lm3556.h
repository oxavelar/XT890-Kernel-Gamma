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
#ifndef _lm3556_H_
#define _lm3556_H_

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/mutex.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <linux/ioctl.h>

#include <linux/atomisp.h>

#define LEDFLASH_LM3556_NAME    "lm3556"
#define LEDFLASH_LM3556_ID      3556

#define	__GP_CORE(offset)	(96 + (offset))
#define	GP_LM3556_FLASH_STROBE	__GP_CORE(65)
#define GP_LM3556_FLASH_RESET	__GP_CORE(66)

#define	v4l2_queryctrl_entry_integer(_id, _name,\
		_minimum, _maximum, _step, \
		_default_value, _flags)	\
	{\
		.id = (_id), \
		.type = V4L2_CTRL_TYPE_INTEGER, \
		.name = _name, \
		.minimum = (_minimum), \
		.maximum = (_maximum), \
		.step = (_step), \
		.default_value = (_default_value),\
		.flags = (_flags),\
	}
#define	v4l2_queryctrl_entry_boolean(_id, _name,\
		_default_value, _flags)	\
	{\
		.id = (_id), \
		.type = V4L2_CTRL_TYPE_BOOLEAN, \
		.name = _name, \
		.minimum = 0, \
		.maximum = 1, \
		.step = 1, \
		.default_value = (_default_value),\
		.flags = (_flags),\
	}

#define	s_ctrl_id_entry_integer(_id, _name, \
		_minimum, _maximum, _step, \
		_default_value, _flags, \
		_s_ctrl, _g_ctrl)	\
	{\
		.qc = v4l2_queryctrl_entry_integer(_id, _name,\
				_minimum, _maximum, _step,\
				_default_value, _flags), \
		.s_ctrl = _s_ctrl, \
		.g_ctrl = _g_ctrl, \
	}

#define	s_ctrl_id_entry_boolean(_id, _name, \
		_default_value, _flags, \
		_s_ctrl, _g_ctrl)	\
	{\
		.qc = v4l2_queryctrl_entry_boolean(_id, _name,\
				_default_value, _flags), \
		.s_ctrl = _s_ctrl, \
		.g_ctrl = _g_ctrl, \
	}

/* Registers */
#define LM3556_CONFIGURATION_REG        0x07
#define LM3556_FLASH_FEATURES_REG       0x08
#define LM3556_CURRENT_CONTROL_REG      0x09
#define LM3556_ENABLE_REG               0x0A
#define LM3556_FLAGS_REG                0x0B

/* Value settings for Flash Time-out Duration*/
#define LM3556_DEFAULT_TIMEOUT          300U
#define LM3556_MIN_TIMEOUT              100U
#define LM3556_MAX_TIMEOUT              800U
#define LM3556_TIMEOUT_STEPSIZE         100U

/* Flash modes */
#define LM3556_MODE_SHUTDOWN            0x00
#define LM3556_MODE_INDICATOR           0x23
#define LM3556_MODE_TORCH               0x02
#define LM3556_MODE_FLASH               0x23

/* Flags */
#define LM3556_FLAG_TIMEOUT                  (1<<0)
#define LM3556_FLAG_THERMAL_SHUTDOWN         (1<<1)
#define LM3556_FLAG_LED_FAULT                (1<<2)
#define LM3556_FLAG_OVP                      (1<<3)
#define LM3556_FLAG_UVLO                     (1<<4)
#define LM3556_FLAG_IVFM                     (1<<5)
#define LM3556_FLAG_NTC_TRIP                 (1<<6)
#define LM3556_FLAG_TX_EVENT                 (1<<7)

/* Percentage <-> value macros */
#define LM3556_MIN_PERCENT                   0U
#define LM3556_MAX_PERCENT                   100U
#define LM3556_CLAMP_PERCENTAGE(val) \
	clamp(val, LM3556_MIN_PERCENT, LM3556_MAX_PERCENT)

#define LM3556_VALUE_TO_PERCENT(v, step)     (((((unsigned long)(v))*(step))+50)/100)
#define LM3556_PERCENT_TO_VALUE(p, step)     (((((unsigned long)(p))*100)+((step)>>1))/(step))

/* Product specific limits
 * TODO: get these from platform data */
#define LM3556_TORCH_MAX_LVL   0x2 /*  140.63mA */
#define LM3556_FLASH_MAX_LVL   0xC /* 1218.75mA */

/* Flash brightness, input is percentage, output is [0..15] */
#define LM3556_FLASH_STEP	\
	((100ul*(LM3556_MAX_PERCENT)+((LM3556_FLASH_MAX_LVL)>>1))/(LM3556_FLASH_MAX_LVL))

#define LM3556_FLASH_DEFAULT_BRIGHTNESS \
	LM3556_VALUE_TO_PERCENT(15, LM3556_FLASH_STEP)

/* Torch brightness, input is percentage, output is [0..7] */
#define LM3556_TORCH_STEP \
	(100*(LM3556_MAX_PERCENT+1)/(LM3556_TORCH_MAX_LVL+1))
#define LM3556_TORCH_DEFAULT_BRIGHTNESS \
	LM3556_VALUE_TO_PERCENT(0, LM3556_TORCH_STEP)

/* Indicator brightness, input is percentage, output is [0..3] */
#define LM3556_INDICATOR_STEP                2500
#define LM3556_INDICATOR_DEFAULT_BRIGHTNESS \
	LM3556_VALUE_TO_PERCENT(3, LM3556_INDICATOR_STEP)

#endif /* _LM3556_H_ */

