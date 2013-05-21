#define DEBUG

#include <linux/delay.h>
#include <asm/intel_scu_ipcutil.h>
#include <media/v4l2-subdev.h>

#include "platform_mt9e013.h"
#include "platform_camera.h"

static int sh833su_power_ctrl(struct v4l2_subdev *sd, int on)
{
	pr_debug("%s %d\n", __func__, on);
	return mt9e013_module_power_ctrl(sd, on);
}

static int sh833su_gpio_ctrl(struct v4l2_subdev *sd, int on)
{
	return 0;
}

static int sh833su_flisclk_ctrl(struct v4l2_subdev *sd, int on)
{
	return 0;
}

static struct camera_sensor_platform_data sh833su_pdata = {
	.gpio_ctrl	= sh833su_gpio_ctrl,
	.flisclk_ctrl	= sh833su_flisclk_ctrl,
	.power_ctrl	= sh833su_power_ctrl,
};

void *sh833su_platform_data(void *info)
{
	return &sh833su_pdata;
}
