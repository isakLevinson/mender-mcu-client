
#define DEF_DBG_MODULE	DBG_MODULE_CMD

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "argtable3/argtable3.h"

#include "esp_event.h"
#include "esp_check.h"

#include "soc/soc_caps.h"
#include "driver/gpio.h"
#include "driver/uart.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "main.h"
#include "cmd.h"
#include "cli.h"

static bool _init(void)
{
    return true;
}

static bool dbgStatus(uint8_t argc, char** argv)
{
    return true;
}

DEBUG_MENU_START(g_menu)
    DEBUG_MENU_DIR("cmd", NULL)
	    DEBUG_MENU_CMD("status",		NULL,		NULL, dbgStatus)
    DEBUG_MENU_DIR_END
DEBUG_MENU_END


void CMD_init(void)
{
    DBG_TREE_add("/",		g_menu);

    _init();
}