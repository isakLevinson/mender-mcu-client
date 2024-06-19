
#define DEF_DBG_MODULE	DBG_MODULE_PUMP

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"


#include "esp_event.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"

#include "soc/soc_caps.h"
#include "driver/mcpwm_timer.h"
#include "driver/mcpwm_oper.h"
#include "driver/mcpwm_cmpr.h"
#include "driver/mcpwm_gen.h"
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "argtable3/argtable3.h"
#include "freertos/task.h"

#include "app.h"

static const uint8_t g_valveGpios[] = {
    9,//v0
    10,
    11,
    12,
    13,
};

#define VALVE_COUNT     (sizeof(g_valveGpios)/sizeof(g_valveGpios[0]))

static void _init(void)
{
    int i;

    for (i=0; i<VALVE_COUNT; i++) {
        gpio_set_direction(g_valveGpios[i], GPIO_MODE_OUTPUT);
        gpio_set_level(g_valveGpios[i], 0);
    }
}

bool _valveOn(uint8_t v, bool on)
{
    if (v >= VALVE_COUNT) {
        return false;
    }

    gpio_set_level(g_valveGpios[v], on);

    return true;
}

static bool dbgValve(uint8_t argc, char** argv)
{

    uint8_t v;
    bool    on;

    if (argc < 3) {
        return false;
    }

    v = strtoul(argv[1], NULL, 10);
    on = strtoul(argv[2], NULL, 10);

    _valveOn(v, on);

    return true;
}

static bool dbgGpio(uint8_t argc, char** argv)
{

    uint8_t gpio;
    char    val;

    if (argc < 3) {
        return false;
    }

    gpio = strtoul(argv[1], NULL, 10);
    val = argv[2][0];

    switch (val) {
        case '0':  
            gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
            gpio_set_level(gpio, 0);
            break;

        case '1':  
            gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
            gpio_set_level(gpio, 1);
            break;

        case 'i':   gpio_set_direction(gpio, GPIO_MODE_INPUT);
            break;

        default:
    }

    return true;
}

static bool dbgStatus(uint8_t argc, char** argv)
{
    return true;
}

DEBUG_MENU_START(g_menu)
    DEBUG_MENU_DIR("app", NULL)
	    DEBUG_MENU_CMD("status",		NULL,		NULL, dbgStatus)
	    DEBUG_MENU_CMD("valve",			NULL,		NULL, dbgValve)
	    DEBUG_MENU_CMD("gpio",			NULL,		NULL, dbgGpio)
    DEBUG_MENU_DIR_END
DEBUG_MENU_END


void APP_init(void)
{
    DBG_TREE_add("/",		g_menu);

    _init();
}