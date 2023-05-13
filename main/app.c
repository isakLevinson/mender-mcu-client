
#define DEF_DBG_MODULE	DBG_MODULE_APP

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "driver/pulse_cnt.h"
#include "driver/gpio.h"

//#include "unity.h"

#include "esp_timer.h"
#include "esp_event.h"
#include "esp_check.h"

#include "soc/soc_caps.h"
#include "argtable3/argtable3.h"

#include "main.h"
#include "cli.h"
#include "adc.h"

#define TIMER_INTERVAL_US   10000
#define TIMER_INTERVAL_MS   (TIMER_INTERVAL_US / 1000)

static EventGroupHandle_t _event_group;
esp_timer_handle_t periodic_timer;

static bool _init(void)
{
    return true;
}

static bool dbgStatus(uint8_t argc, char** argv)
{
    return true;
}

DEBUG_MENU_START(g_menu)
    DEBUG_MENU_DIR("app", NULL)
	    DEBUG_MENU_CMD("status",	NULL,       		            NULL, dbgStatus)
    DEBUG_MENU_DIR_END
DEBUG_MENU_END


void APP_init(void)
{
    DBG_TREE_add("/", g_menu);

    _init();
}