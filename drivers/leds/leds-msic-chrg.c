/*
 * leds-msic-chrg.c - Intel MSIC Charging LED driver
 *
 * Copyright (C) 2012 Motorola Mobility, Inc.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/ipc_device.h>
#include <linux/leds.h>
#include <linux/earlysuspend.h>
#include <asm/intel_scu_ipc.h>

#define MSIC_BATT_CHR_WDTWRITE_ADDR 0x190
#define WDTWRITE_UNLOCK_VALUE       0x01
/*
 * Each LSB of Charger LED PWM register
 * contributes to 0.39% of duty cycle
 */
#define MSIC_CHRG_LED_PWM_REG   0x194

#define MSIC_CHRG_LED_CNTL_REG  0x195
#define MSIC_CHRG_LED_ENBL      (1 << 0)

/* LED DC Current settings */
#define MSIC_CHRG_LED_DCCUR1    0x0 /* 0.5 mA */
#define MSIC_CHRG_LED_DCCUR2    0x1 /* 1.0 mA */
#define MSIC_CHRG_LED_DCCUR3    0x2 /* 2.5 mA */
#define MSIC_CHRG_LED_DCCUR4    0x3 /* 5.0 mA */

/* Charger LED Frequency settings */
#define MSIC_CHRG_LED_FREQ1     0x0 /* 0.25 Hz */
#define MSIC_CHRG_LED_FREQ2     0x1 /* 0.50 Hz */
#define MSIC_CHRG_LED_FREQ3     0x2 /* 1.00 Hz */
#define MSIC_CHRG_LED_FREQ4     0x3 /* 2.00 Hz */

static struct mutex lock;

static void msic_chrg_led_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	int ret;
	uint8_t val;

	mutex_lock(&lock);

	pr_debug("%s: %d\n", __func__, value);
	/* Unlock Charge parameter registers before reading */
	ret = intel_scu_ipc_iowrite8(MSIC_BATT_CHR_WDTWRITE_ADDR,
		WDTWRITE_UNLOCK_VALUE);
	if (ret) {
		pr_err("%s: intel_scu_ipc_iowrite8 error WDTWRITE: %d\n",
			__func__, ret);
		mutex_unlock(&lock);
		return;
	}

	/* Read control register and set enable bit */
	ret = intel_scu_ipc_ioread8(MSIC_CHRG_LED_CNTL_REG, &val);
	if (ret) {
		pr_err("%s: intel_scu_ipc_ioread8 error %d\n", __func__, ret);
		mutex_unlock(&lock);
		return;
	}
	if (value)
		val |= MSIC_CHRG_LED_ENBL;
	else
		val &= (~MSIC_CHRG_LED_ENBL);
	ret = intel_scu_ipc_iowrite8(MSIC_CHRG_LED_CNTL_REG, val);
	if (ret)
		pr_err("%s: intel_scu_ipc_iowrite8 error %d\n", __func__, ret);
	mutex_unlock(&lock);
}

static struct led_classdev charging_led = {
	.name			= "charging",
	.brightness_set		= msic_chrg_led_set,
	.brightness		= LED_OFF,
};

static int __devinit msic_chrg_led_probe(struct ipc_device *ipcdev)
{
	int ret;

	ret = led_classdev_register(&ipcdev->dev, &charging_led);
	if (ret) {
		pr_err("%s: register charging LED failed: %d", __func__, ret);
		return ret;
	}
	mutex_init(&lock);
	msic_chrg_led_set(&charging_led, charging_led.brightness);

	pr_info("%s: finished successfully\n", __func__);
	return 0;
}

static int __devexit msic_chrg_led_remove(struct ipc_device *ipcdev)
{
	msic_chrg_led_set(&charging_led, LED_OFF);
	led_classdev_unregister(&charging_led);

	return 0;
}

static struct ipc_driver msic_chrg_led_driver = {
	.driver = {
		   .name = "msic_chrg_led",
		   .owner = THIS_MODULE,
	},
	.probe = msic_chrg_led_probe,
	.remove = __devexit_p(msic_chrg_led_remove),
};

static int __init msic_chrg_led_init(void)
{
	int ret;

	ret = ipc_driver_register(&msic_chrg_led_driver);
	if (ret)
		pr_err("%s: failed, error %d\n", __func__, ret);
	return ret;
}

static void __exit msic_chrg_led_exit(void)
{
	ipc_driver_unregister(&msic_chrg_led_driver);
}

module_init(msic_chrg_led_init);
module_exit(msic_chrg_led_exit);

MODULE_DESCRIPTION("Intel MSIC Charging LED Driver");
MODULE_LICENSE("GPL v2");
