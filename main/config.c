
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
#include "mdns.h"
#include "wifi.h"
#include "httpd.h"
#include "max30001.h"
#include "time.h"
#include "config.h"
#include "nvs.h"
#include "factory.h"

#define NS_CFG	"cfg"

enum {
	namespace_null,
	namespace_cfg,
} namespace_t;

char* const g_namespaces[] = {
	NULL,
	"cfg",
};

typedef struct {
	char*	key;
	char*	namespace;
	char*	def;
} cfg_arr_t;

#define CFG_ARR(id, ns, d)	[cfg_id_ ## id] = {.key = #id, .namespace = g_namespaces[namespace_ ## ns], .def = d},

static cfg_arr_t g_id[] = {
	CFG_LIST(CFG_ARR)
	{
		.key = NULL, .def = NULL
	}
};

static struct {
	TimerHandle_t		timer;
} g_cfg;

static void _timer(TimerHandle_t pxTimer)
{
	INFO("config _timer\n");
	wss_config_stop();
	WIFI_stopAp();
}

bool CFG_isValidName(char* pName)
{
	uint8_t	i = 1 ; // first one is "INVALID"

	while (g_id[i].key) {
		if (!strcmp(g_id[i].key, pName)) {
			return true;
		}
		i++;
	}

	return false;
}

PARSE_STATUS CFG_parseWssCommand(char* pStr, size_t size)
{
	int ret;
	cJSON* json = NULL;
	const cJSON* object = NULL;
	const cJSON* objectSsid = NULL;
	const cJSON* objectPasswd = NULL;

	json = cJSON_ParseWithLength(pStr, size);
	if (!json) {
		WARN("json parse error\n");
		return PARSE_STATUS_SYNTAX_ERROR;
	}

	object = json;
	while (object) {
		const cJSON* child = object->child;
		INFO("%x, child:%x\n", object, child);

		while (child) {
			if (child->string) {
				bool isValidName = CFG_isValidName(child->string);
				INFO("name:%s %d\n", child->string, isValidName);
				if (!isValidName) {
					WARN("invalid name %s\n", child->string);
					return PARSE_STATUS_UNSUPPORTED_PARAM;
				}
			}

			child = child->next;
		}

		object = object->next;
	}

	object = cJSON_GetObjectItemCaseSensitive(json, "cert");
	if (object) {
		INFO("cert: %s\n", object->valuestring);
		CFG_set(cfg_id_cert,  object->valuestring);
	}

	object = cJSON_GetObjectItemCaseSensitive(json, "sync_dns");
	if (object) {
		INFO("sync_dns: %s\n", object->valuestring);
		CFG_set(cfg_id_sync_dns, object->valuestring);
	}

	object = cJSON_GetObjectItemCaseSensitive(json, "sync_port");
	if (object) {
		INFO("sync_port: %s\n", object->valuestring);
		CFG_set(cfg_id_sync_port, object->valuestring);
	}

	object = cJSON_GetObjectItemCaseSensitive(json, "mender_url");
	if (object) {
		INFO("mender_url: %s\n", object->valuestring);
		CFG_set(cfg_id_ota_url, object->valuestring);
	}

	object = cJSON_GetObjectItemCaseSensitive(json, "mender_token");
	if (object) {
		INFO("mender_token: %s\n", object->valuestring);
		CFG_set(cfg_id_ota_token, object->valuestring);
	}

	object = cJSON_GetObjectItemCaseSensitive(json, "certificate");
	if (object) {
		INFO("certificate: %s\n", object->valuestring);
		CFG_set(cfg_id_cert_pem, object->valuestring);
	}

	object = cJSON_GetObjectItemCaseSensitive(json, "key");
	if (object) {
		INFO("key: %s\n", object->valuestring);
		CFG_set(cfg_id_key_pem, object->valuestring);
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

	objectSsid = cJSON_GetObjectItemCaseSensitive(json, "ssid");
	if (objectSsid) {
		INFO("ssid: %s\n", objectSsid->valuestring);
		objectPasswd = cJSON_GetObjectItemCaseSensitive(json, "passwd");
		if (objectPasswd) {
			bool connected = false;
			int32_t t;
			int32_t t0;
			INFO("passwd: %s\n", objectPasswd->valuestring);
			WIFI_sta_connect(objectSsid->valuestring, objectPasswd->valuestring);

			t0 = TIME_get32();
			do {
				vTaskDelay(100);
				t = TIME_get32();
				if (t - t0 > WIFI_CONNECTION_TIMEOUT) {
					return PARSE_STATUS_INVALID_CREDENTIAL;
				}
				connected = WIFI_isConnected();
				TRACE("#1 %d %d\n", connected, t - t0);
			} while (!connected);
			INFO("CFG_parseWssCommand connected\n");
		}
	}

	INFO("CFG_parseWssCommand: PARSE_STATUS_OK\n");

	ret = xTimerStart(g_cfg.timer, 0);
	if (pdPASS != ret) {
		ERROR("failed to start timer\n");
	}

	return PARSE_STATUS_OK;
}

bool CFG_default(void)
{
	NVS_eraseAll();
	WIFI_sta_disconnect();
	WIFI_startAp();
	wss_config_start();
	return true;
}

bool CFG_get(cfg_id_t id,  char* val, size_t maxSize)
{
	bool	ret;

	if (id >= cfg_id_last) {
		return false;
	}

	ret = NVS_get(g_id[id].namespace, g_id[id].key, val, maxSize);
	if (ret) {
		return true;
	}

	ret = FACTORY_get(g_id[id].key, val, maxSize);
	if (ret) {
		return true;
	}

	if (g_id[id].def) {
		strncpy(val, g_id[id].def, maxSize);
		return true;
	}

	val[0] = '\0';
	return false;
}

bool CFG_set(cfg_id_t id,  char* val)
{
	bool	ret;

	if (id >= cfg_id_last) {
		return false;
	}

	ret = NVS_set(g_id[id].namespace, g_id[id].key, val);
	if (!ret) {
		return false;
	}

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


static bool dbgGet(uint8_t argc, char** argv)
{
	bool    	ret;
	bool    	isAll	= false;
	char*		pIdStr	= NULL;
	char  		str[2048];
	cfg_id_t	id;

// *INDENT-OFF*
	ARGS_ENTRY_BEGIN(args)
		ARGS_ENTRY("a",			ARGS_TYPE_SWITCH,		0,	"all",	&isAll)
		ARGS_ENTRY(NULL,	    ARGS_TYPE_UINT8,		0,	"id",	&id)
	ARGS_ENTRY_END()
// *INDENT-ON*

	ret = ARGS_readValues(argc, argv, args, NULL, NULL);
	if (!ret) {
		return false;
	}

	if (isAll) {
		for (id = 0; id < cfg_id_last; id++) {
			ret = CFG_get(id, str, sizeof(str));
			char* nsStr = "NULL";
			char* keyStr = "NULL";
			if (g_id[id].namespace) {
				nsStr = g_id[id].namespace;
			}

			if (g_id[id].key) {
				keyStr = g_id[id].key;
			}

			if (ret) {
				PRINT("%2d %s %s: %s\n", id, nsStr, keyStr, str);
			} else {

				PRINT("%2d %s %s: NULL\n", id, nsStr, keyStr);
			}
		}

		return true;
	}

	ret = CFG_get(id, str, sizeof(str));
	if (ret) {
		char* nsStr = "NULL";
		char* keyStr = "NULL";

		if (g_id[id].namespace) {
			nsStr = g_id[id].namespace;
		}

		if (g_id[id].key) {
			keyStr = g_id[id].key;
		}
//		PRINT("%s\n", str);
		PRINT("%2d %s %s: %s\n", id, g_id[id].namespace, g_id[id].key, str);
	} else {
		PRINT("NULL\n");
	}

	return true;
}

#if 0
static bool dbgSet(uint8_t argc, char** argv)
{
	bool    ret;
	factory_id id = 0;

	if (argc < 3) {
		return false;
	}

	while (id < factory_id_last) {
		if (!strcmp(argv[1], g_str[id])) {
			break;
		}
		id++;
	}

	if (id >= factory_id_last) {
		ERROR("invalid id\n");
		return false;
	}

	if (g_override[id]) {
		free(g_override[id]);
	}

	g_override[id] = malloc(strlen(argv[2]) + 1);
	if (!g_override[id]) {
		ERROR("failed to allocate %d\n", strlen(argv[2]));
		return true;
	}

	strcpy(g_override[id], argv[2]);

	return true;
}
#endif

// *INDENT-OFF*
DEBUG_MENU_START(g_menu)
	DEBUG_MENU_DIR("config", NULL)
		DEBUG_MENU_CMD("status",    	NULL,		NULL, dbgStatus)
		DEBUG_MENU_CMD("json",	    	NULL,		NULL, dbgJson)
		DEBUG_MENU_CMD("config",		"<json>",	NULL, dbgConfig)
		DEBUG_MENU_CMD("default",		NULL,		NULL, dbgDefault)
		DEBUG_MENU_CMD("startServer",	NULL,		NULL, dbgStartServer)
		DEBUG_MENU_CMD("get",			NULL,		NULL, dbgGet)
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*


void CFG_init(void)
{
	DBG_TREE_add("/",		g_menu);

	g_cfg.timer = xTimerCreate("config", 1000, pdFALSE, NULL, _timer);
}
