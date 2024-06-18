
#define DEF_DBG_MODULE	DBG_MODULE_PUMP

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"


static bool dbgStatus(uint8_t argc, char** argv)
{
    return true;
}

static bool dbgPwm(uint8_t argc, char** argv)
{
    return true;
}

static bool dbgGpio(uint8_t argc, char** argv)
{
    return true;
}

DEBUG_MENU_START(g_menu)
    DEBUG_MENU_DIR("pump", NULL)
	    DEBUG_MENU_CMD("status",		NULL,		NULL, dbgStatus)
	    DEBUG_MENU_CMD("pwm",			NULL,		NULL, dbgPwm)
	    DEBUG_MENU_CMD("gpio",			NULL,		NULL, dbgGpio)
    DEBUG_MENU_DIR_END
DEBUG_MENU_END


void PMP_init(void)
{
    DBG_TREE_add("/",		g_menu);
}