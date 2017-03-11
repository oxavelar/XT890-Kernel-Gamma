/*
 * platform_mt9e013.c: mt9e013 platform data initilization file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#define DEBUG

#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/atomisp_platform.h>
#include <asm/intel_scu_ipcutil.h>
#include <asm/intel-mid.h>
#include <media/v4l2-subdev.h>
#include "platform_camera.h"
#include "platform_mt9e013.h"

static int reset = -1;
static int vprog1_on;

static int on_count;


/*
 * MFLD PR2 primary camera sensor - MT9E013 platform data
 */

int mt9e013_module_power_ctrl(struct v4l2_subdev *sd, int on)
{
	int ret;

	pr_debug("%s %d\n", __func__, on);

	if (on) {
		if (++on_count > 1) {
			pr_debug("%s: already on\n", __func__);
			return 0;
		}
	} else {
		if (on_count == 0) {
			pr_debug("%s: already off\n", __func__);
			return 0;
		}
		if (--on_count > 0) {
			pr_debug("%s: staying on\n", __func__);
			return 0;
		}
	}

	if (reset < 0) {
		ret = camera_sensor_gpio(-1, GP_CAMERA_0_RESET,
					 GPIOF_DIR_OUT, 1);
		if (ret < 0)
			return ret;
		reset = ret;
	}

	if (on) {
		pr_debug("%s ON\n", __func__);

		gpio_set_value(reset, 1);

		usleep_range(2000, 2000);

		ret = gpio_request(160, "cam0_en");
		if (ret) {
			pr_err("%s: failed to request gpio 160\n", __func__);
			return -EIO;
		}
		gpio_export(160, 1);
		ret = gpio_direction_output(160, 1);
		if (ret) {
			pr_err("%s: failed to set gpio160\n", __func__);
			return -EIO;
		}

		if (!vprog1_on) {
			vprog1_on = 1;
			intel_scu_ipc_msic_vprog1(1);
		}

		ret = intel_scu_ipc_osc_clk(OSC_CLK_CAM0, 19200);
		if (ret) {
			pr_err("%s: failed to enable clk %d\n",
					__func__, ret);
			return ret;
		}

		usleep_range(1000, 1000);

		gpio_set_value(reset, 0);

		usleep_range(2000, 2000);

		gpio_set_value(reset, 1);

		usleep_range(2000, 2000);
	} else {
		pr_debug("%s OFF\n", __func__);

		gpio_set_value(reset, 0);

		ret = intel_scu_ipc_osc_clk(OSC_CLK_CAM0, 0);
		if (ret) {
			pr_err("%s: failed to disable clk %d\n",
					__func__, ret);
		}

		if (vprog1_on) {
 			intel_scu_ipc_msic_vprog1(0);
			vprog1_on = 0;
 		}

		gpio_set_value(160, 0);
		gpio_free(160);
	}

	return 0;
}

static int mt9e013_gpio_ctrl(struct v4l2_subdev *sd, int on)
{
	return 0;
}

static int mt9e013_flisclk_ctrl(struct v4l2_subdev *sd, int on)
{
	return 0;
}

static int mt9e013_csi_configure(struct v4l2_subdev *sd, int on)
{
	return camera_sensor_csi(sd, ATOMISP_CAMERA_PORT_PRIMARY, 2,
		ATOMISP_INPUT_FORMAT_RAW_10, atomisp_bayer_order_rggb, on);
}

static bool mt9e013_low_fps(void)
{
	return (board_id == MFLD_BID_LEX) ? true : false;
}

static struct camera_sensor_platform_data mt9e013_sensor_platform_data = {
	.gpio_ctrl	= mt9e013_gpio_ctrl,
	.flisclk_ctrl	= mt9e013_flisclk_ctrl,
	.power_ctrl	= mt9e013_module_power_ctrl,
	.csi_cfg	= mt9e013_csi_configure,
	.low_fps	= mt9e013_low_fps,
};

void *mt9e013_platform_data(void *info)
{
	return &mt9e013_sensor_platform_data;
}
