
#define DEF_DBG_MODULE	DBG_MODULE_APP

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "argtable3/argtable3.h"
#include "cmd_decl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_event.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "soc/soc_caps.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"

#include "main.h"
#include "cli.h"
#include "motor.h"
#include "enc.h"



static struct {
    int target;
    int speed;
} g_app;

static void _task(void *arg)
{
    bool    ret;
    int     degree;
    int     delta;

    INFO("APP Ready.\n");

    while(true) {
#if 0        
        ret = ENC_get(&degree);

        if (ret) {
            delta = ABS(degree - g_app.target);
            if (delta < 5) {
                MOT_setSpeed(0);
                if (g_app.speed) {
                    INFO("stopping at %d, d=%d\n", degree, delta);
                    g_app.speed = 0;
                }
            }
        } else {
            MOT_setSpeed(g_app.speed);
        }
#endif
        vTaskDelay(10);
    }

    vTaskDelete(NULL);
}

static bool _init(void)
{
    int ret;

    ret = xTaskCreate(_task, "app", 4096, NULL, 7, NULL);
    if (ret != pdPASS) {
        //ERROR
        return false;
    }

    return true;
}

static bool dbgTarget(uint8_t argc, char** argv)
{
    if (argc < 2) {
        return false;
    }

    g_app.target    = strtol(argv[1], NULL, 10);

    if (argc >= 3) {
        g_app.speed     = strtol(argv[2], NULL, 10);
    }

    MOT_setSpeed(g_app.speed);

    return true;
}

static bool dbgStatus(uint8_t argc, char** argv)
{
    bool    ret;
    int     deg;

    ret = ENC_get(&deg);

    PRINT("target : %d\n", g_app.target);
    PRINT("speed  : %d\n", g_app.speed);

    if (ret) {
        PRINT("current: %d (d=%d)\n", deg, ABS(deg - g_app.target));
    } else {
        PRINT("current: Uninit\n");
    }
    return true;
}

DEBUG_MENU_START(g_menu)
    DEBUG_MENU_DIR("app", NULL)
	    DEBUG_MENU_CMD("status",	NULL,		NULL, dbgStatus)
	    DEBUG_MENU_CMD("target",	"<target> [speed]",		NULL, dbgTarget)
    DEBUG_MENU_DIR_END
DEBUG_MENU_END


void APP_init(void)
{
    DBG_TREE_add("/", g_menu);

    _init();
}