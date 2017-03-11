#define DEBUG

#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/atomisp_platform.h>
#include <asm/intel_scu_ipcutil.h>
#include <media/v4l2-subdev.h>

#include "platform_camera.h"

static int reset = -1;
static int pwrdn = -1;

/*
 * MFLD secondary camera sensor - ov7736 platform data
 */
static int ov7736_power_ctrl(struct v4l2_subdev *sd, int on)
{
	int ret;

	pr_debug("%s %d\n", __func__, on);

	/* Note here, there maybe a workaround to avoid I2C SDA issue */
	if (pwrdn < 0) {
		ret = camera_sensor_gpio(-1, GP_CAMERA_1_POWER_DOWN,
					GPIOF_DIR_OUT, 1);
		if (ret < 0)
			return ret;

		pwrdn = ret;
	}

	if (reset < 0) {
		ret = camera_sensor_gpio(-1, GP_CAMERA_1_RESET,
					 GPIOF_DIR_OUT, 1);
		if (ret < 0)
			return ret;

		reset = ret;
	}

	if (on) {
		pr_debug("%s ON\n");

		gpio_set_value(pwrdn, 1);
		gpio_set_value(reset, 0);

		ret = gpio_request(33, "cam1_en");
		if (ret) {
			pr_err("%s: failed to request gpio 33\n", __func__);
			return -EIO;
		}
		gpio_export(33, 1);
		ret = gpio_direction_output(33, 1);
		if (ret) {
			pr_err("%s: failed to set high 33\n", __func__);
			return -EIO;
		}

		usleep_range(1000, 1000);

		gpio_set_value(pwrdn, 0);

		gpio_set_value(reset, 1);

		ret = intel_scu_ipc_osc_clk(OSC_CLK_CAM1, 19200);
		if (ret) {
			pr_err("%s: failed to enable clk %d\n",
					__func__, ret);
			return ret;
		}

		msleep(20);

	} else {
		pr_debug("%s OFF\n");

		gpio_set_value(pwrdn, 1);
		gpio_set_value(reset, 0);

		ret = intel_scu_ipc_osc_clk(OSC_CLK_CAM1, 0);
		if (ret) {
			pr_err("%s: failed to disable clk %d\n",
					__func__, ret);
		}

		gpio_set_value(33, 0);
		gpio_free(33);
	}

	return 0;
}

static int ov7736_gpio_ctrl(struct v4l2_subdev *sd, int flag)
{
	return 0;
}

static int ov7736_flisclk_ctrl(struct v4l2_subdev *sd, int flag)
{
	return 0;
}

static int ov7736_csi_configure(struct v4l2_subdev *sd, int on)
{
	/* soc sensor, there is no raw bayer order (set to -1) */
	return camera_sensor_csi(sd, ATOMISP_CAMERA_PORT_SECONDARY, 1,
		ATOMISP_INPUT_FORMAT_YUV422_8, -1, on);
}

static struct camera_sensor_platform_data ov7736_sensor_platform_data = {
	.gpio_ctrl	= ov7736_gpio_ctrl,
	.flisclk_ctrl	= ov7736_flisclk_ctrl,
	.power_ctrl	= ov7736_power_ctrl,
	.csi_cfg	= ov7736_csi_configure,
};

void *ov7736_platform_data(void *info)
{
	return &ov7736_sensor_platform_data;
}
