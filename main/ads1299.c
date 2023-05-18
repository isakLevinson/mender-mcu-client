
#define DEF_DBG_MODULE	DBG_MODULE_ADC

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

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_timer.h"

#include "esp_system.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "main.h"
#include "cli.h"
#include "ads1299.h"


static void _init(void)
{

}

static bool dbgStatus(uint8_t argc, char** argv)
{
    PRINT("status\n");
    return true;
}

static bool dbgRd(uint8_t argc, char** argv)
{
    return true;
}

static bool dbgWr(uint8_t argc, char** argv)
{
    return true;
}

DEBUG_MENU_START(g_menu)
    DEBUG_MENU_DIR("ads1299", NULL)
	    DEBUG_MENU_CMD("status",	NULL,      NULL, dbgStatus)
	    DEBUG_MENU_CMD("rd",	    NULL,      NULL, dbgRd)
	    DEBUG_MENU_CMD("wr",	    NULL,      NULL, dbgWr)
   DEBUG_MENU_DIR_END
DEBUG_MENU_END


void ADS1299_init(void)
{
    DBG_TREE_add("/", g_menu);

    _init();
}