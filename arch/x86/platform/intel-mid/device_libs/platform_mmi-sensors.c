#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/akm8963.h>
#include <linux/lis3dh.h>
#include <linux/ct406.h>
#include <linux/gpio.h>

#include <asm/intel-mid.h>

#include "platform_mmi-sensors.h"
#include "platform_a1026.h"


#define LIS3DH_INT1_NAME "accel_int"
#define LIS3DH_INT2_NAME "accel_2"
#define CT406_INT_NAME "als_prox_int"
#define AKM8963_INT_NAME "mag_int"


/*
 * AKM8963 Magnetometer
 */

static struct akm8963_platform_data mp_akm8963_pdata = {
	.layout = 1,
	.outbit = 1,
	.gpio_DRDY = 0,
	.gpio_RST = 0,
};


/* 
 * LIS3DH Acceleromter
 */

static int lis3dh_initialization(void)
{
	return 0;
}

static void lis3dh_exit(void)
{
}

static int lis3dh_power_on(void)
{
	return 0;
}

static int lis3dh_power_off(void)
{
	return 0;
}

static struct lis3dh_platform_data mp_lis3dh_pdata = {
	.init = lis3dh_initialization,
	.exit = lis3dh_exit,
	.power_on = lis3dh_power_on,
	.power_off = lis3dh_power_off,

	.min_interval = 1,
	.poll_interval = 200,
	.g_range = 8,
};


/*
 * CT406 Light/Prox sensor
 */

static struct ct406_platform_data mp_ct406_pdata = {
	.regulator_name = "",
	.prox_samples_for_noise_floor = 0x05,

	.ct405_prox_saturation_threshold = 0x0208,
	.ct405_prox_covered_offset = 0x008c,
	.ct405_prox_uncovered_offset = 0x0046,
	.ct405_prox_recalibrate_offset = 0x0046,

	.ct406_prox_saturation_threshold = 0x0208,
	.ct406_prox_covered_offset = 0x0096,
	.ct406_prox_uncovered_offset = 0x006e,
	.ct406_prox_recalibrate_offset = 0x0046,
	.ct406_prox_pulse_count = 0x04,
	.ct406_prox_offset = 0x00,

	.ip_prox_limit = 0x200,
	.ip_als_limit = 10,
};


static struct i2c_board_info pr2_i2c_bus5_devs[] = {
	/* !!!
	 *
	 * HACK: audience should remain as the first item in the list
	 * since it is addressed by mmi_register_board_i2c_devs below.
	 *
	 * !!!
	 */
	{
		.type		= "audience_es305",
		.irq		= 0xff,
		.addr		= 0x3e,
	},
	{
		.type		= "lis3dh",
		.irq		= 0xff,
		.addr		= 0x18,
		.platform_data	= &mp_lis3dh_pdata,
	},
	{
		.type		= "akm8963",
		.irq		= 0xff,
		.addr		= 0x0C,
		.platform_data	= &mp_akm8963_pdata,
	},
	{
		.type		= "ct406",
		.addr		= 0x39,
		.platform_data = &mp_ct406_pdata,
	},
};


void __init mmi_sensors_init(void)
{
	int gpio = -1;
	int ret;

	pr_debug("[Sensors] %s\n",__func__);

	pr_debug("[Sensors] LIS3DH\n");

	gpio = get_gpio_by_name(LIS3DH_INT1_NAME);
	if (gpio != -1) {
		ret = gpio_request(gpio, "lis3dh accel int1");
		if (!ret) {
			gpio_direction_input(gpio);
			ret = gpio_export(gpio, 0);
			if (ret) {
				pr_err("%s: lis3dh int1 export failed\n",
					__func__);
				gpio_free(gpio);
			}
		} else {
			pr_err("%s: lis3dh int1 gpio request failed\n",
				__func__);
		}
	}

	gpio = get_gpio_by_name(LIS3DH_INT2_NAME);
	if (gpio != -1) {
		ret = gpio_request(gpio, "lis3dh accel int2");
		if (!ret) {
			gpio_direction_input(gpio);
			ret = gpio_export(gpio, 0);
			if (ret) {
				pr_err("%s: lis3dh int2 export failed\n",
					__func__);
				gpio_free(gpio);
			}
		} else {
			pr_err("%s: lis3dh int2 gpio request failed\n",
				__func__);
		}
	}

	pr_debug("[Sensors] AK8963\n");

	gpio = get_gpio_by_name(AKM8963_INT_NAME);
	if (gpio <= 0) {
		pr_err("%s: AKM8963 GPIO not defined\n", __func__);
	} else {
		gpio_request(gpio, "akm8963 irq");
		gpio_direction_input(gpio);
		gpio_export(gpio , 0);
		mp_akm8963_pdata.gpio_DRDY = gpio;
	}

	mp_akm8963_pdata.layout = 2;
	mp_akm8963_pdata.outbit = 0;

	pr_debug("[Sensors] CT406\n");

	gpio = get_gpio_by_name(CT406_INT_NAME);
	if (gpio <= 0) {
		pr_err("%s: CT406 GPIO not defined\n", __func__);
	} else {
		mp_ct406_pdata.irq = gpio_to_irq(gpio);
		gpio_request(gpio, "ct406 proximity int");
		gpio_direction_input(gpio);
		gpio_export(gpio , 0);
	}
}

void __init mmi_register_board_i2c_devs(void)
{
	pr2_i2c_bus5_devs[0].platform_data = audience_platform_data(NULL);

	i2c_register_board_info(5, pr2_i2c_bus5_devs,
			ARRAY_SIZE(pr2_i2c_bus5_devs));
}
