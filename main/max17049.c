
#define DEF_DBG_MODULE	DBG_MODULE_FG

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static void _init(void)
{
	
}


static bool dbgRd(uint8_t argc, char** argv)
{
	return true;
}

static bool dbgWr(uint8_t argc, char** argv)
{
	return true;
}

static bool dbgStatus(uint8_t argc, char** argv)
{
	return true;
}


// *INDENT-OFF*
DEBUG_MENU_START(g_menu)
	DEBUG_MENU_DIR("fg",	NULL)
		DEBUG_MENU_CMD("status",	NULL,		NULL, dbgStatus)
		DEBUG_MENU_CMD("r",			NULL,		NULL, dbgRd)
		DEBUG_MENU_CMD("w",			NULL,		NULL, dbgWr)
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*


void max17049_init(void)
{
	DBG_TREE_add("/", g_menu);

	_init();
}
