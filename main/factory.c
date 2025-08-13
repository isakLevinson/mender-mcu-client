
#define DEF_DBG_MODULE	DBG_MODULE_FACTORY
#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include "esp_partition.h"
#include "esp_flash.h"
#include "esp_efuse_chip.h"
#include "esp_efuse.h"
#include "driver/gpio.h"
#include "factory.h"

#include "cJSON.h"
#include "nvs.h"

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

static void _freeObject(void)
{
	free(g_pBuf);
	g_pBuf = NULL;
}

static bool _getFactoryObjectStr(char* pObject, char* val, size_t maxSize)
{
	esp_err_t               err;
	cJSON*            json = NULL;
	const cJSON*            object = NULL;
	const esp_partition_t*  partition = NULL;

	partition = find_partition(0x40, 0x01, NULL);
	if (!partition) {
		WARN("config partition not found\n");
		return false;
	}

	if (!g_pBuf) {
		INFO("addr:0x%x size:0x%x spi:0x%x\n", partition->address, partition->size, partition->flash_chip);
		g_pBuf = malloc(partition->size);
		if (!g_pBuf) {
			ERROR("failed allocating %d bytes for partition\n", partition->size);
			return false;
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
		TRACE("%s not found\n", pObject);
		TRACE_BUF(NULL, PRINT_BUF_STYLE_ASC_SIZE_NL, g_pBuf, partition->size);
		goto error;
	}

	TRACE("%s: %s\n", pObject, object->valuestring);

	if (strlen(object->valuestring) > maxSize) {
		goto error;
	}

	if (val) {
		strncpy(val, object->valuestring, maxSize);
	}

	cJSON_Delete(json);
	return true;

error:
	cJSON_Delete(json);
	return false;
}

bool FACTORY_get(char* key,  char* val, size_t maxSize)
{
	bool ret;

	ret = _getFactoryObjectStr(key, val, maxSize);
	if (!ret) {
		TRACE("key %s not found\n", key);
		return false;
	}

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

static bool dbgGetObject(uint8_t argc, char** argv)
{
	bool    ret;
	char  	str[2048];

	if (argc < 2) {
		return false;
	}

	ret = FACTORY_get(argv[1], str, sizeof(str));
	if (ret) {
		PRINT("%s\n", str);
	}

	return true;
}

static bool dbgFreeObject(uint8_t argc, char** argv)
{
	_freeObject();
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

// *INDENT-OFF*
DEBUG_MENU_START(g_menu)
	DEBUG_MENU_DIR("factory", NULL)
		DEBUG_MENU_CMD("status",    	NULL,		NULL, dbgStatus)
		DEBUG_MENU_CMD("findPart",  	NULL,		NULL, dbgFindPart)
		DEBUG_MENU_CMD("rd",	    	NULL,		NULL, dbgRead)
		DEBUG_MENU_CMD("get", 			NULL,		NULL, dbgGetObject)
		DEBUG_MENU_CMD("freeObject",	NULL,		NULL, dbgFreeObject)
		DEBUG_MENU_CMD("gpio",			NULL,		NULL, dbgGpio)
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*

void FACTORY_init(void)
{
	DBG_TREE_add("/",		g_menu);
}
