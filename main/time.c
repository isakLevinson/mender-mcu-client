
#define DEF_DBG_MODULE	DBG_MODULE_SPI

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_timer.h"
#include "esp_system.h"

#include "main.h"

static struct {
	int64_t	offset64;
	int64_t	lastUpdated;
} g_timerDb;

uint64_t TIME_set64(int64_t time)
{
	int64_t t;

	t = esp_timer_get_time();

	g_timerDb.offset64		= t - time;
	g_timerDb.lastUpdated	= time;

	return t;
}

void TIME_getUpdateTime(int64_t* o_pTime)
{
	*o_pTime = g_timerDb.lastUpdated;
}

void TIME_get64(int64_t* o_pTime)
{
	int64_t t;

	t = esp_timer_get_time();
	*o_pTime = t - g_timerDb.offset64;
}

int32_t TIME_get32(void)
{
	int64_t t;

	t = esp_timer_get_time();

	return (int32_t)((t/1000) & 0xffffffff);
}


