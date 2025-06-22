

#define DEF_DBG_MODULE	DBG_MODULE_WIFI
#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include "esp_netif_sntp.h"
#include "esp_sntp.h"

static void sntp_sync_time_cb(struct timeval* tv)
{
	int64_t	t;
	int32_t t32;

	INFO("sntp_sync_time_cb %d %d sec\n", tv->tv_sec, (uint32_t)(tv->tv_usec));

	TIME_set64((int64_t)tv->tv_sec * 1000000);
	TIME_get64(&t);

	t32 = TIME_get32();
	INFO("time: %d.%d\n", (int32_t)(t / 1000000), (int32_t)(t % 1000000));
	INFO("t32 : %d\n", t32);
}

static void _init(void)
{
	esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");

	esp_netif_sntp_init(&config);

	sntp_set_time_sync_notification_cb(sntp_sync_time_cb);

	//	esp_sntp_init();
	sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
	//sntp_set_sync_interval(3600000);
}

bool ntp_restart(void)
{
	bool ret = sntp_restart();
	INFO("sntp_restart %d\n", ret);

	return true;
}

static bool dbgInit(uint8_t argc, char** argv)
{
	_init();
	return true;
}

static bool dbgStatus(uint8_t argc, char** argv)
{
	sntp_sync_status_t status = sntp_get_sync_status();
	uint8_t	i;

	switch (status) {
		case SNTP_SYNC_STATUS_COMPLETED:
			PRINT("SNTP_SYNC_STATUS_COMPLETED\n");
			break;
		case SNTP_SYNC_STATUS_RESET:
			PRINT("SNTP_SYNC_STATUS_RESET\n");
			break;
		case SNTP_SYNC_STATUS_IN_PROGRESS:
			PRINT("SNTP_SYNC_STATUS_IN_PROGRESS\n");
			break;
		default:
			PRINT("sntp_get_sync_status %d\n", status);
	}

	sntp_sync_mode_t mode = sntp_get_sync_mode();
	switch (mode) {
		case SNTP_SYNC_MODE_IMMED:
			PRINT("SNTP_SYNC_MODE_IMMED\n");
			break;
		case SNTP_SYNC_MODE_SMOOTH:
			PRINT("SNTP_SYNC_MODE_SMOOTH\n");
			break;
		default:
			PRINT("sntp_get_sync_mode %d\n", mode);
	}

	uint32_t	interval = sntp_get_sync_interval();
	PRINT("interval: %d\n", interval);

	for (i = 0; i < 8; i++) {
		char* srvr = esp_sntp_getservername(i);
		if (srvr) {
			uint8_t reachability = sntp_getreachability(i);
			PRINT("%d: %s %d\n", i, srvr, reachability);
		}
	}

	return true;
}

static bool dbgRestart(uint8_t argc, char** argv)
{
	ntp_restart();
	return true;
}

// *INDENT-OFF*
DEBUG_MENU_START(g_menu)
	DEBUG_MENU_DIR("ntp", NULL)
		DEBUG_MENU_CMD("status",	        NULL,		NULL, dbgStatus)
		DEBUG_MENU_CMD("init",		        NULL,		NULL, dbgInit)
		DEBUG_MENU_CMD("restart",	        NULL,		NULL, dbgRestart)
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*

void ntp_init(void)
{
	DBG_TREE_add("/wifi", g_menu);

	_init();
}
