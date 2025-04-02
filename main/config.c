
#define DEF_DBG_MODULE	DBG_MODULE_NVS

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include "esp_partition.h"
#include "esp_flash.h"
#include "esp_efuse_chip.h"
#include "esp_efuse.h"
#include "driver/gpio.h"

#include "cJSON.h"
#include "nvs.h"
#include "mdns.h"
#include "wifi.h"
#include "wss.h"
#include "max30001.h"

bool CFG_parseWssCommand(char* pStr, size_t size)
{
	cJSON* json = NULL;
	const cJSON* object = NULL;
	const cJSON* objectSsid = NULL;
	const cJSON* objectPasswd = NULL;

	json = cJSON_ParseWithLength(pStr, size);
	if (!json) {
		WARN("json parse error\n");
		return false;
	}

	object = json;
	while (object) {
		const cJSON* child = object->child;
		INFO("%x, child:%x\n", object, child);

		while (child) {
				if (child->string) {
					bool isValidName = NVS_isValidName(child->string);
					INFO("name:%s %d\n", child->string, isValidName);
					if (!isValidName) {
						WARN("invalid name %s\n", child->string);
						return false;
					}
				}

				child = child->next;
		}

		object = object->next;
	}

	objectSsid = cJSON_GetObjectItemCaseSensitive(json, "ssid");
	if (objectSsid) {
		INFO("ssid: %s\n", objectSsid->valuestring);
		objectPasswd = cJSON_GetObjectItemCaseSensitive(json, "passwd");
		if (objectPasswd) {
			INFO("passwd: %s\n", objectPasswd->valuestring);
			INFO("setting ssid and passwd\n");
			WIFI_sta_connect(objectSsid->valuestring, objectPasswd->valuestring);
		}
	}

	object = cJSON_GetObjectItemCaseSensitive(json, "cert");
	if (object) {
		INFO("cert: %s\n", object->valuestring);
		NVS_set(nvs_id_cert,  object->valuestring);
	}

	object = cJSON_GetObjectItemCaseSensitive(json, "sync_dns");
	if (object) {
		INFO("sync_dns: %s\n", object->valuestring);
		NVS_set(nvs_id_sync_dns, object->valuestring);
	}

	object = cJSON_GetObjectItemCaseSensitive(json, "sync_port");
	if (object) {
		INFO("sync_port: %s\n", object->valuestring);
		NVS_set(nvs_id_sync_port, object->valuestring);
	}

	object = cJSON_GetObjectItemCaseSensitive(json, "mender_url");
	if (object) {
		INFO("mender_url: %s\n", object->valuestring);
		NVS_set(nvs_id_ota_url, object->valuestring);
	}

	object = cJSON_GetObjectItemCaseSensitive(json, "mender_token");
	if (object) {
		INFO("mender_url: %s\n", object->valuestring);
		NVS_set(nvs_id_ota_token, object->valuestring);
	}

	object = cJSON_GetObjectItemCaseSensitive(json, "wr_reg");
	if (object) {
		if (cJSON_IsArray(object)) {
			const cJSON* element;
			PRINT("array:\n");
			PRINT("number: %f\n", object->valuedouble);
			cJSON_ArrayForEach(element, object) {
				const cJSON* reg;
				const cJSON* val;

				reg = cJSON_GetObjectItemCaseSensitive(element, "reg");
				if (!reg) {
					PRINT("reg not found\n");
				} else if (cJSON_IsNumber(reg)) {
					PRINT("reg: %f\n", reg->valuedouble);
				}
				val = cJSON_GetObjectItemCaseSensitive(element, "val");
				if (!val) {
					PRINT("reg not found\n");
				} else if (cJSON_IsNumber(val)) {
					PRINT("val: %f\n", val->valuedouble);
				}

				if (reg && val && cJSON_IsNumber(reg) && cJSON_IsNumber(val)) {
					INFO("reg:%f, val:%f\n", reg->valuedouble, val->valuedouble);
					max30001_write_reg(reg->valuedouble, val->valuedouble);
				}
			}
		}
	}

	return true;
}

bool CFG_default(void)
{
	NVS_eraseAll();
	WIFI_sta_disconnect();
	WIFI_startAp();
	
	wss_config_start();
	return true;
}

static bool dbgStatus(uint8_t argc, char** argv)
{
	return true;
}

static bool dbgJson(uint8_t argc, char** argv)
{
	cJSON* json = NULL;
	const cJSON* object = NULL;

	if (argc < 2) {
		return false;
	}

	PRINT("parsing <%s>\n", argv[1]);

	//cJSON_ParseWithLength
	json = cJSON_Parse(argv[1]);
	if (!json) {
		PRINT("json parse error\n");
		return true;
	}

	object = json;
	while (object) {
		const cJSON* child = object->child;
		PRINT("%x, child:%x\n", object, child);

		while (child) {
			if (child->string) {
				PRINT("name:%s\n", child->string);
			}
			child = child->next;
		}

		object = object->next;
	}

	if (argc < 3) {
		return true;
	}

	PRINT("searching <%s>\n", argv[2]);
	object = cJSON_GetObjectItemCaseSensitive(json, argv[2]);
	if (!object) {
		PRINT("object not found\n");
		return false;
	}

	if (cJSON_IsString(object)) {
		PRINT("string: %s\n", object->valuestring);
	}

	if (cJSON_IsNumber(object)) {
		PRINT("number: %f\n", object->valuedouble);
	}

	if (cJSON_IsArray(object)) {
		const cJSON* element;
		int i = 0;
		PRINT("array:\n");
		PRINT("number: %f\n", object->valuedouble);
		cJSON_ArrayForEach(element, object) {
			PRINT("%d: \n", i);
			if (argc >= 3) {
				const cJSON* val1;
				val1 = cJSON_GetObjectItemCaseSensitive(element, argv[3]);
				if (!val1) {
					PRINT("object not found %s\n", argv[3]);
				} else if (cJSON_IsNumber(val1)) {
					PRINT("number: %f\n", val1->valuedouble);
				}
			}
			i++;
		}
	}

	return true;
}

static bool dbgConfig(uint8_t argc, char** argv)
{
	if (argc < 2) {
		return false;
	}

	CFG_parseWssCommand(argv[1], strlen(argv[1]));
	return true;
}

static bool dbgDefault(uint8_t argc, char** argv)
{
	if (argc < 2) {
		PRINT("please do default 1\n");
		return false;
	}

	if ('1' != argv[1][0]) {
		return false;
	}
	
	CFG_default();
	return true;
}

static bool dbgStartServer(uint8_t argc, char** argv)
{
	wss_config_start();
	return true;
}

// *INDENT-OFF*
DEBUG_MENU_START(g_menu)
	DEBUG_MENU_DIR("config", NULL)
		DEBUG_MENU_CMD("status",    	NULL,		NULL, dbgStatus)
		DEBUG_MENU_CMD("json",	    	NULL,		NULL, dbgJson)
		DEBUG_MENU_CMD("config",		"<json>",	NULL, dbgConfig)
		DEBUG_MENU_CMD("default",		NULL,		NULL, dbgDefault)
		DEBUG_MENU_CMD("startServer",	NULL,		NULL, dbgStartServer)
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*


void CFG_init(void)
{
	DBG_TREE_add("/",		g_menu);
}
