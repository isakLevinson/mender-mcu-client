
#define DEF_DBG_MODULE	DBG_MODULE_NVS

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include "esp_partition.h"
#include "esp_flash.h"
#include "driver/gpio.h"

#include "cJSON.h"
#include "nvs.h"
#include "mdns.h"
#include "wifi.h"
#include "wss.h"
#include "max30001.h"

static const esp_partition_t* g_partition = NULL;
static char*    g_pBuf;

static const esp_partition_t* find_partition(esp_partition_type_t type, esp_partition_subtype_t subtype, const char* name)
{
	//    INFO("Find partition with type %s, subtype %s, label %s...", get_type_str(type), get_subtype_str(subtype),
	//                    name == NULL ? "NULL (unspecified)" : name);

	const esp_partition_t* part  = esp_partition_find_first(type, subtype, name);

	if (!part) {
		ERROR("partition not found\n");
		return NULL;
	}

	TRACE("found partition '%s' at offset 0x%x with size 0x%x\n", part->label, part->address, part->size);

	return part;
}

bool CFG_parseWssCommand(char* pStr, size_t size)
{
	cJSON* json = NULL;
	const cJSON* object = NULL;
	const cJSON* objectSsid = NULL;
	const cJSON* objectPasswd = NULL;

	json = cJSON_ParseWithLength(pStr, size);
	if (!json) {
		PRINT("json parse error\n");
		return false;
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
		NVS_set_certificate(object->valuestring);
	}

	object = cJSON_GetObjectItemCaseSensitive(json, "sync_dns");
	if (object) {
		INFO("sync_dns: %s\n", object->valuestring);
		NVS_set_sync_dns(object->valuestring);
	}

	object = cJSON_GetObjectItemCaseSensitive(json, "sync_port");
	if (object) {
		INFO("sync_port: %s\n", object->valuestring);
		NVS_set_sync_port(object->valuestring);
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

static void _freeObject(void)
{
	free(g_pBuf);
	g_pBuf = NULL;
}

static const cJSON* _getFactoryObjectStr(char* pObject, char** ppVal)
{
	esp_err_t               err;
	const cJSON*            json = NULL;
	const cJSON*            object = NULL;
	const esp_partition_t*  partition = NULL;

	partition = find_partition(0x40, 0x01, NULL);
	if (!partition) {
		WARN("config partition not found\n");
		return NULL;
	}

	if (!g_pBuf) {
		INFO("addr:0x%x size:0x%x spi:0x%x\n", partition->address, partition->size, partition->flash_chip);
		g_pBuf = malloc(partition->size);
		if (!g_pBuf) {
			ERROR("failed allocating %d bytes for partition\n", partition->size);
			return NULL;
		}

		err = esp_partition_read(partition, 0, g_pBuf, partition->size);
		if (ESP_OK != err) {
			ERROR("read failed %d\n", err);
			goto error;
		}
	}

	//TRACE_BUF(NULL, PRINT_BUF_STYLE_ASC_SIZE_NL, g_pBuf, partition->size);

	json = cJSON_ParseWithLength(g_pBuf, partition->size);
	if (!json) {
		ERROR("json parse error\n");
		goto error;
	}

	object = cJSON_GetObjectItemCaseSensitive(json, pObject);
	if (!object) {
		WARN("%s not found\n", pObject);
		INFO_BUF(NULL, PRINT_BUF_STYLE_ASC_SIZE_NL, g_pBuf, partition->size);
		goto error;
	}

	INFO("%s: %s\n", pObject, object->valuestring);

	if (ppVal) {
		*ppVal = object->valuestring;
	}

	return object;

error:
	return NULL;
}


bool CFG_factoryGetPrivateKey(char** o_ppStr)
{
	const cJSON*  object;

	object = _getFactoryObjectStr("private_key", o_ppStr);
	if (!object) {
		return false;
	}

	return true;
}

bool CFG_factoryGetPublicKey(char** o_ppStr)
{
	const cJSON*  object;

	object = _getFactoryObjectStr("public_key", o_ppStr);
	if (!object) {
		return false;
	}

	return true;
}

bool CFG_factoryGetCertificate(char** o_ppStr)
{
	const cJSON*  object;

	object = _getFactoryObjectStr("certificate", o_ppStr);
	if (!object) {
		return false;
	}

	return true;
}

bool CFG_factoryGetManufacturingDate(char** o_ppStr)
{
	const cJSON*  object;

	object = _getFactoryObjectStr("manufacturing_date", o_ppStr);
	if (!object) {
		return false;
	}

	return true;
}

bool CFG_factoryGetSn(char** o_ppStr)
{
	const cJSON*  object;

	object = _getFactoryObjectStr("sn", o_ppStr);
	if (!object) {
		return false;
	}

	return true;
}

bool CFG_factoryGetHwRevision(char** o_ppStr)
{
	const cJSON*  object;

	object = _getFactoryObjectStr("hw_revision", o_ppStr);
	if (!object) {
		return false;
	}

	return true;
}

bool CFG_factoryGetModel(char** o_ppStr)
{
	const cJSON*  object;

	object = _getFactoryObjectStr("model", o_ppStr);
	if (!object) {
		return false;
	}

	return true;
}

bool CFG_default(void)
{
	NVS_eraseAll();
	WIFI_sta_disconnect();
	WIFI_startAp();
	wss_start_config();

	return true;
}

static bool dbgFindPart(uint8_t argc, char** argv)
{
	uint8_t type;
	uint8_t subType;
	const esp_partition_t* part;

	if (argc < 3) {
		return false;
	}

	type = strtol(argv[1], NULL, 16);
	subType = strtol(argv[2], NULL, 16);

	part = find_partition(type, subType, NULL);
	if (!part) {
		PRINT("partition not found\n");
		return true;
	}

	PRINT("addr:0x%x size:0x%x spi:0x%x\n", part->address, part->size, part->flash_chip);

	return true;
}

static bool dbgStatus(uint8_t argc, char** argv)
{
	g_partition = find_partition(0x40, 0x01, NULL);
	if (!g_partition) {
		PRINT("config partition not found\n");
		return true;
	}

	PRINT("addr:0x%x size:0x%x spi:0x%x\n", g_partition->address, g_partition->size, g_partition->flash_chip);

	return true;
}

static bool dbgRead(uint8_t argc, char** argv)
{
	esp_err_t   err;
	uint32_t    addr;
	uint8_t     buf[32];

	if (argc < 2) {
		return false;
	}

	if (!g_partition) {
		PRINT("no partition set\n");
		return true;
	}

	if (!g_partition->flash_chip) {
		PRINT("partition doesnt contain valid flash chip\n");
		return true;
	}

	addr = strtol(argv[1], NULL, 16);

	//err = esp_flash_read(g_partition->flash_chip, buf, addr, sizeof(buf));
	err = esp_partition_read(g_partition, addr, buf, sizeof(buf));
	if (ESP_OK != err) {
		PRINT("read failed %d\n", err);
		return true;
	}

	PRINT_BUF(NULL, PRINT_BUF_STYLE_HEX_SIZE_NL, buf, sizeof(buf));

	return true;
}

static bool dbgJson(uint8_t argc, char** argv)
{
	cJSON* json = NULL;
	const cJSON* object = NULL;

	if (argc < 3) {
		return false;
	}

	//cJSON_ParseWithLength
	json = cJSON_Parse(argv[1]);
	if (!json) {
		PRINT("json parse error\n");
		return false;
	}

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

static bool dbgGetObject(uint8_t argc, char** argv)
{
	bool    ret;
	const cJSON* object;
	bool    isAll			= false;
	bool    isPrivate		= false;
	bool    isPublic		= false;
	bool    isCertificate	= false;
	bool    isDate			= false;
	bool    isSn			= false;
	bool    isRevision		= false;
	bool    isModel			= false;
	char*   pStr = NULL;

// *INDENT-OFF*
	ARGS_ENTRY_BEGIN(args)
		ARGS_ENTRY("a",			ARGS_TYPE_SWITCH,		0,	"",				    &isAll)
		ARGS_ENTRY("priv",		ARGS_TYPE_SWITCH,		0,	"",				    &isPrivate)
		ARGS_ENTRY("pub",		ARGS_TYPE_SWITCH,		0,	"",				    &isPublic)
		ARGS_ENTRY("cert",		ARGS_TYPE_SWITCH,		0,	"",				    &isCertificate)
		ARGS_ENTRY("date",		ARGS_TYPE_SWITCH,		0,	"",				    &isDate)
		ARGS_ENTRY("sn",		ARGS_TYPE_SWITCH,		0,	"",				    &isSn)
		ARGS_ENTRY("rev",		ARGS_TYPE_SWITCH,		0,	"",				    &isRevision)
		ARGS_ENTRY("model",		ARGS_TYPE_SWITCH,		0,	"",				    &isModel)
		ARGS_ENTRY(NULL,	    ARGS_TYPE_STRING,		0,	"generic object",   &pStr)
	ARGS_ENTRY_END()
// *INDENT-ON*

	ret = ARGS_readValues(argc, argv, args, NULL, NULL);
	if (!ret) {
		return false;
	}

	if (isAll) {
		isPrivate		 = true;
		isPublic		= true;
		isCertificate	= true;
		isDate			= true;
		isSn			= true;
		isRevision		= true;
		isModel			= true;
	}

	if (isPrivate) {
		CFG_factoryGetPrivateKey(NULL);
	}
	if (isPublic) {
		CFG_factoryGetPublicKey(NULL);
	}
	if (isCertificate) {
		CFG_factoryGetCertificate(NULL);
	}
	if (isDate) {
		CFG_factoryGetManufacturingDate(NULL);
	}
	if (isSn) {
		CFG_factoryGetSn(NULL);
	}
	if (isRevision) {
		CFG_factoryGetHwRevision(NULL);
	}
	if (isModel) {
		CFG_factoryGetModel(NULL);
	}
	if (pStr) {
		object = _getFactoryObjectStr(argv[1], NULL);
		if (!object) {
			PRINT("failed to get object\n");
			return true;
		}

		PRINT("%s\n", object->valuestring);
	}

	return true;
}

static bool dbgFreeObject(uint8_t argc, char** argv)
{
	_freeObject();
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

static bool dbgGpio(uint8_t argc, char** argv)
{
	uint8_t gpio;
	char    val;

	if (argc < 3) {
		return false;
	}

	gpio = strtoul(argv[1], NULL, 10);
	val = argv[2][0];

	switch (val) {
		case '0':
			gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
			gpio_set_level(gpio, 0);
			break;

		case '1':
			gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
			gpio_set_level(gpio, 1);
			break;

		case 'i':
			gpio_set_direction(gpio, GPIO_MODE_INPUT);
			val = gpio_get_level(gpio);
			PRINT("%d\n", val);
			break;

		default:
	}

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

// *INDENT-OFF*
DEBUG_MENU_START(g_menu)
	DEBUG_MENU_DIR("config", NULL)
		DEBUG_MENU_CMD("status",    NULL,		NULL, dbgStatus)
		DEBUG_MENU_CMD("findPart",  NULL,		NULL, dbgFindPart)
		DEBUG_MENU_CMD("rd",	    NULL,		NULL, dbgRead)
		DEBUG_MENU_CMD("json",	    NULL,		NULL, dbgJson)
		DEBUG_MENU_CMD("getObject", NULL,		NULL, dbgGetObject)
		DEBUG_MENU_CMD("freeObject",NULL,		NULL, dbgFreeObject)
		DEBUG_MENU_CMD("config",	"<json>",	NULL, dbgConfig)
		DEBUG_MENU_CMD("gpio",		NULL,		NULL, dbgGpio)
		DEBUG_MENU_CMD("default",	NULL,		NULL, dbgDefault)
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*


void CFG_init(void)
{
	DBG_TREE_add("/",		g_menu);
}
