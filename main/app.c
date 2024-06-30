
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
#include "pump.h"
#include "adc.h"
#include "cli.h"


static const uint8_t g_valveGpios[] = {
    9,//v0
    10,
    11,
    12,
    13,
};

#define VALVE_COUNT     (sizeof(g_valveGpios)/sizeof(g_valveGpios[0]))

static struct {
    struct {
        uint16_t    pmpValveDelay;
        uint8_t     histeresisHigh;
        uint8_t     histeresisLow;
    } cfg;

    bool    loopActive;
    int16_t press[4];
    int16_t target[4];
    int8_t  pressurizeState[4];
} g_app = {
    .cfg = {
        .pmpValveDelay  = 100,
        .histeresisHigh = 5,
        .histeresisLow  = 5,
    },
    .loopActive = true,
};

bool _valveOn(uint8_t v, bool on)
{
    if (v >= VALVE_COUNT) {
        return false;
    }

    gpio_set_level(g_valveGpios[v], on);

    return true;
}


static void _pressurize(uint8_t ch, int dir)
{
    if (g_app.pressurizeState[ch] == dir) {
        return;
    }

    switch (dir) {
        case 0: 
            PMP_on(ch, 0);
            _valveOn(ch, 0);
            break;

        case 1: 
            PMP_on(ch, 1);
            vTaskDelay(g_app.cfg.pmpValveDelay);
            _valveOn(ch, 1);
            break;

        case -1: 
            PMP_on(ch, 0);
            _valveOn(ch, 1);
            break;

        default:
    }
    g_app.pressurizeState[ch] = dir;
}

static void _task(void *arg)
{
    uint8_t i;

    while (true) {
        if (!g_app.loopActive) {
            continue;
        }

        ADC_getPressure(g_app.press);
        for (i=0; i<1; i++) {
            if (g_app.target[i] - g_app.press[i] > g_app.cfg.histeresisHigh) {
                _pressurize(i, 1);
            } else if (g_app.target[i] - g_app.press[i] < -g_app.cfg.histeresisLow) {
                _pressurize(i, -1);
            } else {
                _pressurize(i, 0);
            }
        }

        vTaskDelay(100);
    }
}

static void _init(void)
{
    int i;
    int ret;

    for (i=0; i<VALVE_COUNT; i++) {
        gpio_set_direction(g_valveGpios[i], GPIO_MODE_OUTPUT);
        gpio_set_level(g_valveGpios[i], 0);
    }

    ret = xTaskCreate(_task, "app", 8192, NULL, 3, NULL);
    if (ret != pdPASS) {
        ERROR("create task %s failed\n", "app");
        return;
    }
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

static bool dbgCuff(uint8_t argc, char** argv)
{
    bool    ret;
    uint8_t cuff;
    int8_t  op;
    char    c;
    int16_t press[4];

    if (argc < 3)  {
        return false;
    }

    cuff = strtol(argv[1], NULL, 10);
    op = strtol(argv[2], NULL, 10);

    _pressurize(cuff, op);

    do {
        ADC_getPressure(press);
        INFO("%3d %3d %3d %3d\n", press[0], press[1], press[2], press[3]);

        vTaskDelay(100);
        ret = CLI_getc(&c);
    } while (!ret);


    return true;
}

static bool dbgTarget(uint8_t argc, char** argv)
{
    if (argc < 5) {
        return false;
    }

    g_app.target[0] = strtol(argv[1], NULL, 10);
    g_app.target[1] = strtol(argv[2], NULL, 10);
    g_app.target[2] = strtol(argv[3], NULL, 10);
    g_app.target[3] = strtol(argv[4], NULL, 10);

    return true;
}

static bool dbgStatus(uint8_t argc, char** argv)
{
    PRINT("press: %3d %3d %3d %3d\n", g_app.press[0], g_app.press[1], g_app.press[2], g_app.press[3]);
    return true;
}

static bool dbgCfg(uint8_t argc, char** argv)
{
    bool    ret;

// *INDENT-OFF*
	ARGS_ENTRY_BEGIN(args)
		ARGS_ENTRY("hh",		ARGS_TYPE_UINT8,	0,	"histeresis high",	&g_app.cfg.histeresisHigh)
		ARGS_ENTRY("hl",		ARGS_TYPE_UINT8,	0,	"histeresis low",	&g_app.cfg.histeresisLow)
		ARGS_ENTRY("pd",		ARGS_TYPE_UINT16,	0,	"pump delay",   	&g_app.cfg.pmpValveDelay)
	ARGS_ENTRY_END()
// *INDENT-ON*

	ret = ARGS_readValues(argc, argv, args, NULL, NULL);
	if (!ret) {
		return false;
	}

    PRINT("hh: %d\n", g_app.cfg.histeresisHigh);
    PRINT("hl: %d\n", g_app.cfg.histeresisLow);
    PRINT("pd: %d\n", g_app.cfg.pmpValveDelay);

    return true;
}

DEBUG_MENU_START(g_menu)
    DEBUG_MENU_DIR("app", NULL)
	    DEBUG_MENU_CMD("status",		NULL,		NULL, dbgStatus)
	    DEBUG_MENU_CMD("cfg",   		NULL,		NULL, dbgCfg)
	    DEBUG_MENU_CMD("valve",			NULL,		NULL, dbgValve)
	    DEBUG_MENU_CMD("gpio",			NULL,		NULL, dbgGpio)
        DEBUG_MENU_CMD("cuff",			NULL,		NULL, dbgCuff)
        DEBUG_MENU_CMD("target",    	NULL,		NULL, dbgTarget)
    DEBUG_MENU_DIR_END
DEBUG_MENU_END


void APP_init(void)
{
    DBG_TREE_add("/",		g_menu);

    _init();
}