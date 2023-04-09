
#define DEF_DBG_MODULE	DBG_MODULE_ADC

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
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "iperf.h"
#include "esp_coexist.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "soc/soc_caps.h"
#include "driver/gpio.h"

#include "main.h"
#include "adc.h"






static void _init(void)
{

}

static bool dbgStatus(uint8_t argc, char** argv)
{
    return true;
}

DEBUG_MENU_START(g_menu)
    DEBUG_MENU_DIR("adc", NULL)
	    DEBUG_MENU_CMD("status",			NULL,		NULL, dbgStatus)
    DEBUG_MENU_DIR_END
DEBUG_MENU_END


void ADC_init(void)
{
    DBG_TREE_add("/",		g_menu);

    _init();
}