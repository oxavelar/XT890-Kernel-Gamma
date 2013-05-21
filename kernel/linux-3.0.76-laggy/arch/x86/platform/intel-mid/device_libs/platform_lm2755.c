#include <linux/types.h>
#include <linux/leds-lm2755.h>

void *lm2755_led_platform_data(void *info)
{
	static struct lm2755_platform_data lm2755_data = {
		.led_names = {"red", "green", "blue"},
		.rgb_name = "rgb",
		.red_group =   LM2755_GROUP_1,
		.green_group = LM2755_GROUP_2,
		.blue_group =  LM2755_GROUP_3,
		.clock_mode = LM2755_CLOCK_EXT,
		.charge_pump_mode = LM2755_CHARGE_PUMP_BEFORE,
		.min_tstep = 976562,
		.max_level = LM2755_MAX_LEVEL,
		.enable = NULL,
	};

	return &lm2755_data;
}
