
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

typedef struct {
	char*	key;
	char*	namespace;
	char*	def;
	char*	temp;
	config_type_t	type;
	uint8_t	count;
} cfg_item_t;

enum {
	namespace_null,
	namespace_cfg,
} namespace_t;

char* const g_namespaces[] = {
	NULL,
	"cfg",
};

#define CFG_ARR(id, ns, t, d)	[cfg_id_ ## id] = {	\
        .key = #id,	\
        .namespace = g_namespaces[namespace_ ## ns],	\
        .def = d,	\
        .type = config_type_ ## t,	\
    },

static cfg_item_t g_id[] = {
	CFG_LIST(CFG_ARR)
	{
		.key = NULL, .def = NULL, .temp = NULL,
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

static int32_t _findPartialId(char* key)
{
	int32_t id = -1;
	int32_t i;

	for (i = 0; i < cfg_id_last; i++) {
		if (strstr(g_id[i].key, key)) {
			if (id >= 0) {
				INFO("more than one match\n");
				return -1;
			}
			id = i;
		}
	}

	INFO("found id %d\n", id);

	if (id < 0) {
		id = strtol(key, NULL, 10);
		if (0 == id)  {
			INFO("strtol failed\n");
			return -1;
		}
		INFO("using explicit id %d\n", id);
	}

	INFO("id=%d\n", id);
	return id;
}

int32_t _findId(char* pName)
{
	uint8_t	i;

	for (i = 0; i < cfg_id_last; i++) {
		if (!strcmp(g_id[i].key, pName)) {
			return i;
		}
	}

	return -1;
}

cfg_status_t _checkConfigValidity(void)
{
	uint8_t	i;

	for (i = 0; i < cfg_id_last; i++) {
		INFO("%-16s: %2d %d\n", g_id[i].key, g_id[i].type, g_id[i].count);
		if (g_id[i].count > 1) {
			ERROR("more than one %s\n", g_id[i].key);
			return cfg_status_syntax_error;
		}

		if ((g_id[i].namespace) && (g_id[i].type == config_type_mandatory) && (!g_id[i].count)) {
			ERROR("missing mandatory param %s\n", g_id[i].key);
			return cfg_status_missing_param;
		}

		if ((g_id[i].type != config_type_mandatory) && (g_id[i].type != config_type_optional)) {
			if (g_id[i].count) {
				ERROR("no allowed param %s\n", g_id[i].key);
				return cfg_status_unsupported_param;
			}
		}
	}

	return cfg_status_ok;
}


cfg_item_t* _findEntry(char* pName)
{
	int32_t	id =  _findId(pName);
	if (id < 0) {
		return NULL;
	}
	return &g_id[id];
}

static bool _setTemporary(cfg_id_t id,  char* val)
{
	if (g_id[id].temp) {
		free(g_id[id].temp);
	}

	size_t len = strlen(val);

	//INFO("allocating new temporary %d\n", len);
	g_id[id].temp = malloc(len + 1);
	if (!g_id[id].temp) {
		ERROR("failed to allocate %d\n", len);
		return true;
	}

	//INFO("strcpy to %x\n", g_id[id].temp);
	strcpy(g_id[id].temp, val);

	return true;
}

static void _clearCount(void)
{
	uint8_t i;

	for (i = 0; i < ARR_SIZE(g_id); i++) {
		g_id[i].count = 0;
	}
}

cfg_status_t CFG_parseWssCommand(char* pStr, size_t size)
{
	int ret;
	cfg_status_t status;

	cJSON* root = NULL;
	const cJSON* item = NULL;
	char	ssid[32];
	char	passwd[32];

	root = cJSON_ParseWithLength(pStr, size);
	if (!root) {
		WARN("json parse error\n");
		status = cfg_status_syntax_error;
		goto end;
	}

	_clearCount();

	INFO("validating fields\n");
	cJSON_ArrayForEach(item, root) {
		if (cJSON_IsString(item)) {
			cfg_item_t*  cfg = _findEntry(item->string);

			if (cfg) {
				cfg->count++;
				INFO("%s: %d\n", cfg->key, cfg->type);
			} else {
				WARN("unrecognized param %s\n", item->string);
				status = cfg_status_missing_param;
				goto end;
			}
		}
	}

	status = _checkConfigValidity();

	if (cfg_status_ok != status) {
		goto end;
	}

	cJSON_ArrayForEach(item, root) {
		if (cJSON_IsString(item)) {
			ret = CFG_setByName(item->string, item->valuestring);
			if (!ret) {
				status = cfg_status_syntax_error;
				goto end;
			}
		}
	}

	ret = CFG_get(cfg_id_ssid, ssid, sizeof(ssid));
	if (!ret) {
		ERROR("ssid not set\n");
		status = cfg_status_syntax_error;
		goto end;
	}

	ret = CFG_get(cfg_id_passwd, passwd, sizeof(passwd));
	if (!ret) {
		ERROR("passwd not set\n");
		status = cfg_status_syntax_error;
		goto end;
	}

	ret = WIFI_sta_connect(ssid, passwd);
	if (!ret) {
		ERROR("WIFI_sta_connectfailed\n");
		status = cfg_status_syntax_error;
		goto end;
	}

#if 0
	object = cJSON_GetObjectItemCaseSensitive(json, "wr_reg");
	if (object) {
		if (cJSON_IsArray(object)) {sta	
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
#endif

	if (cfg_status_ok == status) {
		ret = xTimerStart(g_cfg.timer, 0);
		if (pdPASS != ret) {
			ERROR("failed to start timer\n");
		}
	}

end:
	if (root) {
		cJSON_Delete(root);
	}

	return status;
}

bool CFG_factoryReset(void)
{
	NVS_eraseNamespace(NVS_NAMESPACE);
	WIFI_sta_disconnect();
	WIFI_startAp();
	wss_config_start();
	return true;
}

bool CFG_getEx(cfg_id_t id,  char* val, size_t maxSize, cfg_location_t* location)
{
	bool	ret;

	if (id >= cfg_id_last) {
		ERROR("invalid id %d\n", id);
		if (location) {
			*location = cfg_location_invalid;
		}
		return false;
	}

	if (g_id[id].temp) {
		strncpy(val, g_id[id].temp, maxSize);
		if (location) {
			*location = cfg_location_temporary;
		}
		return true;
	}

	ret = NVS_get(g_id[id].namespace, g_id[id].key, val, maxSize);
	if (ret) {
		if (location) {
			*location = cfg_location_nvs;
		}
		return true;
	}

	ret = FACTORY_get(g_id[id].key, val, maxSize);
	if (ret) {
		if (location) {
			*location = cfg_location_factory;
		}
		return true;
	}

	if (g_id[id].def) {
		strncpy(val, g_id[id].def, maxSize);
		if (location) {
			*location = cfg_location_default;
		}
		return true;
	}

	if (location) {
		*location = cfg_location_none;
	}

	TRACE("key %s not found\n", g_id[id].key);
	if (val && maxSize) {
		val[0] = '\0';
	}
	return false;
}

bool CFG_get(cfg_id_t id,  char* val, size_t maxSize)
{
	return CFG_getEx(id, val, maxSize, NULL);
}

bool CFG_set(cfg_id_t id,  char* val)
{
	if (id >= cfg_id_last) {
		return false;
	}

	//INFO("CFG_set %d %s\n", id, val);

	if (g_id[id].namespace) {
		INFO("calling CFG_set\n");
		NVS_set(g_id[id].namespace, g_id[id].key, val);
	} else {
		_setTemporary(id, val);
	}

	return true;
}

bool CFG_setByName(char* key,  char* val)
{
	bool	ret;
	int32_t id = _findId(key);

	if (id < 0)  {
		return false;
	}
	ret = CFG_set(id, val);
	if (!ret) {
		return false;
	}
	return true;
}

bool CFG_del(cfg_id_t id)
{
	if (id >= cfg_id_last) {
		return false;
	}

	if (g_id[id].namespace) {
		NVS_del(g_id[id].namespace, g_id[id].key);
	} else {
		if (g_id[id].temp) {
			free(g_id[id].temp);
			g_id[id].temp = NULL;
		}
	}

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

	CFG_factoryReset();
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
	char  		str[3000];
	cfg_id_t	id;
	cfg_location_t	location;
	char*		key = NULL;

// *INDENT-OFF*
	ARGS_ENTRY_BEGIN(args)
		ARGS_ENTRY("a",			ARGS_TYPE_SWITCH,		0,	"all",	&isAll)
		ARGS_ENTRY(NULL,	    ARGS_TYPE_STRING,		0,	"key",	&key)
	ARGS_ENTRY_END()
// *INDENT-ON*

	ret = ARGS_readValues(argc, argv, args, NULL, NULL);
	if (!ret) {
		return false;
	}

	if (isAll) {
		for (id = 0; id < cfg_id_last; id++) {
			ret = CFG_getEx(id, str, sizeof(str), &location);
			char* nsStr = "NUL";
			char* keyStr = "NULL";
			if (g_id[id].namespace) {
				nsStr = g_id[id].namespace;
			}

			if (g_id[id].key) {
				keyStr = g_id[id].key;
			}

			if (ret) {
				char* locStr = "";
				switch (location) {
					case cfg_location_invalid:
						locStr = "inv ";
						break;
					case cfg_location_none:
						locStr = "none";
						break;
					case cfg_location_default:
						locStr = "def ";
						break;
					case cfg_location_factory:
						locStr = "fact";
						break;
					case cfg_location_nvs:
						locStr = "nvs ";
						break;
					case cfg_location_temporary:
						locStr = "temp";
						break;
				};

				PRINT("%2d %s %s %-16s: (%d)", id, nsStr, locStr, keyStr, strlen(str));
				PRINT_BUF(NULL,	PRINT_BUF_STYLE_ASC_SIZE_NL, str, MIN(64, strlen(str)));
			} else {
				PRINT("%2d %s none %-16s\n", id, nsStr, keyStr);
			}
		}

		return true;
	}

	if (!key) {
		return false;
	}

	id = _findPartialId(key);
	if (id < 0) {
		return false;
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

		PRINT("%2d %s %s: \n", id, nsStr, keyStr);
		PRINT_BUF("",	PRINT_BUF_STYLE_ASC_SIZE_NL | PRINT_BUF_STYLE_FORMAT_ASC, str, strlen(str));
	} else {
		PRINT("NULL\n");
	}

	return true;
}

static bool dbgSet(uint8_t argc, char** argv)
{
	bool    ret;
	bool	temp	= false;
	bool	nvs		= false;
	int32_t	id = -1;
	char*	key = NULL;
	char*	val = NULL;

// *INDENT-OFF*
	ARGS_ENTRY_BEGIN(args)
		ARGS_ENTRY("t",		ARGS_TYPE_SWITCH,	0,	"store in temporary",	&temp)
		ARGS_ENTRY("n",		ARGS_TYPE_SWITCH,	0,	"store in nvs",			&nvs)
		ARGS_ENTRY(NULL,	ARGS_TYPE_STRING,	1,	"key",   				&key)
		ARGS_ENTRY(NULL,	ARGS_TYPE_STRING,	1,	"value",   				&val)
	ARGS_ENTRY_END()
// *INDENT-ON*

	ret = ARGS_readValues(argc, argv, args, NULL, NULL);
	if (!ret) {
		return false;
	}

	if (id >= cfg_id_last) {
		ERROR("invalid id\n");
		return false;
	}

	if (!val) {
		return false;
	}

	id = _findPartialId(key);
	if (id < 0) {
		return false;
	}

	CFG_set(id, val);

	return true;
}

static bool dbgDel(uint8_t argc, char** argv)
{
	bool    ret;
	bool	temp	= false;
	bool	nvs		= false;
	char*	key		= NULL;
	int32_t	id 		= -1;

// *INDENT-OFF*
	ARGS_ENTRY_BEGIN(args)
		ARGS_ENTRY("t",		ARGS_TYPE_SWITCH,	0,	"store in temporary",	&temp)
		ARGS_ENTRY("n",		ARGS_TYPE_SWITCH,	0,	"store in nvs",			&nvs)
		ARGS_ENTRY(NULL,	ARGS_TYPE_STRING,	1,	"key",   				&key)

	ARGS_ENTRY_END()
// *INDENT-ON*

	ret = ARGS_readValues(argc, argv, args, NULL, NULL);
	if (!ret) {
		return false;
	}

	if (id >= cfg_id_last) {
		ERROR("invalid id\n");
		return false;
	}

	id = _findPartialId(key);
	if (id < 0) {
		return false;
	}

	CFG_del(id);

	return true;
}

// *INDENT-OFF*
DEBUG_MENU_START(g_menu)
	DEBUG_MENU_DIR("config", NULL)
		DEBUG_MENU_CMD("json",	    	NULL,		NULL, dbgJson)
		DEBUG_MENU_CMD("config",		"<json>",	NULL, dbgConfig)
		DEBUG_MENU_CMD("default",		NULL,		NULL, dbgDefault)
		DEBUG_MENU_CMD("startServer",	NULL,		NULL, dbgStartServer)
		DEBUG_MENU_CMD("get",			NULL,		NULL, dbgGet)
		DEBUG_MENU_CMD("set",			NULL,		NULL, dbgSet)
		DEBUG_MENU_CMD("del",			NULL,		NULL, dbgDel)
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*


void CFG_init(void)
{
	DBG_TREE_add("/",		g_menu);

	g_cfg.timer = xTimerCreate("config", 1000, pdFALSE, NULL, _timer);
}
