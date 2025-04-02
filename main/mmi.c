
#define DEF_DBG_MODULE	DBG_MODULE_LED

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include <stdio.h>

#include "driver/gpio.h"
#include "time.h"
#include "wifi.h"
#include "factory.h"
#include "config.h"
#include "led_strip.h"
#include "max17049.h"
#include "nvs.h"

static struct {
	led_strip_handle_t led_strip;
	uint8_t  r;
	uint8_t  g;
	uint8_t  b;
	uint16_t interval;
	uint16_t onTime;
	bool     on;
	int32_t  switchTime;
	bool     isConfigurated;
} g_led;

static bool _update(int8_t r, int8_t g, int8_t b)
{
	led_strip_set_pixel(g_led.led_strip, 0, r, g, b);
	led_strip_refresh(g_led.led_strip);

	return true;
}

static void  _handleBlink(int32_t time)
{
	if (!g_led.interval) {
		g_led.on = true;
	} else {
		if (g_led.on)  {
			if (time - g_led.switchTime > g_led.onTime) {
				g_led.on = false;
			}
		} else {
			if (time - g_led.switchTime > g_led.interval) {
				g_led.on = true;
				g_led.switchTime = time;
			}
		}
	}

	if (g_led.on) {
		_update(g_led.r, g_led.g, g_led.b);
	} else {
		_update(0, 0, 0);
	}
}

static void _task(void* arg)
{
	bool    ret;
	int32_t time;

	ret = FACTORY_factoryGetSn(NULL);
	if (!ret) {
		ERROR("no SN in factory storage. Halting on error\n");
		g_led.r = 100;
		g_led.g = 0;
		g_led.b = 0;
		g_led.interval = 0;
		time = TIME_get32();
		_handleBlink(time);
		while (true) {
			vTaskDelay(10);
		}
	}

	while (true) {
		vTaskDelay(10);
		time = TIME_get32();
		_handleBlink(time);

#if CONFIG_BUILD_TYPE_PNU
		if (!g_led.isConfigurated) {
			char    ssid[32];
			char    passwd[32];
			ret = NVS_get_ssid(ssid, passwd);
			if (ret) {
				g_led.isConfigurated = true;
			}
		}

		if (!g_led.isConfigurated) {
			g_led.r = 100;
			g_led.g = 100;
			g_led.b = 100;
			g_led.interval = 0;
		} else {
			bool isConnected = WIFI_isConnected();
			if (isConnected) {
				bool     err = false;
				uint16_t soc;

				ret = fg_get_soc(&soc);
				if (!ret) {
					soc = 0;
					err = true;
				}

				if (err) {
					g_led.r = 100;
					g_led.g = 0;
					g_led.b = 0;
					g_led.interval  = 0;
				} else if (soc < 15)  {
					g_led.r = 100;
					g_led.g = 0;
					g_led.b = 0;
					g_led.interval  = 500;
					g_led.onTime    = 100;
				} else if (soc < 30) {
					g_led.r = 100;
					g_led.g = 0;
					g_led.b = 0;
					g_led.interval  = 1000;
					g_led.onTime    = 200;
				} else {
					g_led.r = 0;
					g_led.g = 100;
					g_led.b = 0;
					g_led.interval = 0;
				}
			} else {
				g_led.r = 0;
				g_led.g = 100;
				g_led.b = 0;
				g_led.interval  = 1000;
				g_led.onTime    = 200;
			}
		}
#endif
		// TODO: find better place
		static int32_t  pressTime;
		static bool     trig = false;
		gpio_set_direction(GPIO_BOOT_BUTTON, GPIO_MODE_INPUT);
		bool val = gpio_get_level(GPIO_BOOT_BUTTON);

		if (!trig && !val) {
			pressTime = time;
			trig = true;
		}

		if (val) {
			if (trig) {
				if (time - pressTime < BUTTOR_PRESS_TIME_FACTORY_RESET) {
					INFO("press is too short %d\n", time - pressTime);
				}
			}
			trig = false;
		}

		if (trig) {
			if (time - pressTime >= BUTTOR_PRESS_TIME_FACTORY_RESET) {
				INFO("resetting to default\n");
				CFG_default();
				g_led.isConfigurated = false;
				trig = false;
			}
		}
	}
}

static bool _init(void)
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
	ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &g_led.led_strip);
	if (ESP_OK != ret) {
		return false;
	}

	ret = xTaskCreate(_task, "led", 8192, NULL, 3, NULL);
	if (ret != pdPASS) {
		ERROR("create task %s failed\n", "led");
		return false;
	}

	return true;
}

static bool dbgSet(uint8_t argc, char** argv)
{
	if (argc < 6) {
		return false;
	}

	g_led.r           = strtoul(argv[1], NULL, 10);
	g_led.g           = strtoul(argv[2], NULL, 10);
	g_led.b           = strtoul(argv[3], NULL, 10);
	g_led.interval    = strtoul(argv[4], NULL, 10);
	g_led.onTime      = strtoul(argv[5], NULL, 10);

	return true;
}

static bool dbgFactory(uint8_t argc, char** argv)
{
	if (argc < 1) {
		return false;
	}

	if ('1' != argv[1][0]) {
		return false;
	}

	CFG_default();
	return true;
}

static bool dbgStatus(uint8_t argc, char** argv)
{
	PRINT("rgb: %d %d %d\n", g_led.r, g_led.g, g_led.b);
	PRINT("interval: %d / %d\n", g_led.onTime, g_led.interval);
	return true;
}

// *INDENT-OFF*
DEBUG_MENU_START(g_menu)
	DEBUG_MENU_DIR("mmi", NULL)
		DEBUG_MENU_CMD("status",	NULL,		NULL, dbgStatus)
		DEBUG_MENU_CMD("set",	    NULL,		NULL, dbgSet)
		DEBUG_MENU_CMD("factory",	"<1>",		NULL, dbgFactory)
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*

bool MMI_init(void)
{
	DBG_TREE_add("/", g_menu);

	_init();
	return true;
}
