
#define DEF_DBG_MODULE	DBG_MODULE_ADC

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"


#include <stdio.h>

#include "time.h"
#include "led_strip.h"

static struct {
    led_strip_handle_t led_strip;
    uint8_t  r;
    uint8_t  g;
    uint8_t  b;
    uint16_t interval;
    uint16_t onTime;
    bool     on;
    int32_t  switchTime;
} g_led;


static bool _update(int8_t r, int8_t g, int8_t b)
{
    led_strip_set_pixel(g_led.led_strip, 0, r, g, b);
    led_strip_refresh(g_led.led_strip);

    return true;
}

static void _task(void* arg)
{
    int32_t time;
	while (true) {
		vTaskDelay(10);
        time = TIME_get32();
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
            _update(g_led.r, g_led.g, g_led.g);
        } else {
            _update(0, 0, 0);
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

	ret = xTaskCreate(_task, "streamer", 8192, NULL, 3, NULL);
	if (ret != pdPASS) {
		ERROR("create task %s failed\n", "streamer");
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

// *INDENT-OFF*
DEBUG_MENU_START(g_menu)
	DEBUG_MENU_DIR("led", NULL)
		DEBUG_MENU_CMD("set",	NULL,		NULL, dbgSet)
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*

bool LED_init(void)
{
	DBG_TREE_add("/", g_menu);

    _init();
    return true;
}
