
#define DEF_DBG_MODULE	DBG_MODULE_ADC

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdkconfig.h"
#include "led_strip.h"


static led_strip_handle_t led_strip;


bool LED_set(int8_t r, int8_t g, int8_t b)
{
	led_strip_set_pixel(led_strip, 0, r, g, b);
	led_strip_refresh(led_strip);

	return true;
}

bool LED_init(void)
{
	int ret;
	led_strip_config_t strip_config = {
		.strip_gpio_num = GPIO_LED,
		.max_leds = 1, // at least one LED on board
	};

	led_strip_rmt_config_t rmt_config = {
		.resolution_hz = 10 * 1000 * 1000, // 10MHz
		.flags.with_dma = false,
	};
	ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
	if (ESP_OK != ret) {
		return false;
	}

	return true;
}
