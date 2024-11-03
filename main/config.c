
#define DEF_DBG_MODULE	DBG_MODULE_NVS

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include "esp_partition.h"
#include "esp_flash.h"
#include "cJSON.h"
#include "nvs.h"
#include "mdns.h"
#include "wifi.h"

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

    INFO("found partition '%s' at offset 0x%x with size 0x%x\n", part->label, part->address, part->size);

    return part;
}

bool CFG_parseWssCommand(char* pStr, size_t size)
{
    cJSON *json = NULL;
    const cJSON *object = NULL;
    const cJSON *objectSsid = NULL;
    const cJSON *objectPasswd = NULL;

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
            sta_connect(objectSsid->valuestring, objectPasswd->valuestring);
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

    object = cJSON_GetObjectItemCaseSensitive(json, "mdns");
    if (object) {
        INFO("mdns: %s\n", object->valuestring);
        NVS_set_mdns(object->valuestring);
        mdns_hostname_set(object->valuestring);
    }

    return true;
}

static void _freeObject(void)
{
    free(g_pBuf);
    g_pBuf = NULL;
}

static cJSON* _getFactoryObject(char* pObject)
{
    bool                    ret = true;
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
            return NULL;
        }
    }

    TRACE_BUF(NULL, PRINT_BUF_STYLE_ASC_SIZE_NL, g_pBuf, partition->size);

    json = cJSON_ParseWithLength(g_pBuf, partition->size);    
    if (!json) {
        ERROR("json parse error\n");
        return NULL;
    }

    object = cJSON_GetObjectItemCaseSensitive(json, pObject);
    if (object) {
        INFO("%s: %s\n", pObject, object->valuestring);
    }

    return object;
}


bool CFG_factoryGetPrivateKey(char* o_pStr)
{
    cJSON*  object;

    object = _getFactoryObject("private_key");
    if (!object) {
        return false;
    }

    return true;
}

bool CFG_factoryGetPublicKey(char* o_pStr)
{
    cJSON*  object;

    object = _getFactoryObject("public_key");
    if (!object) {
        return false;
    }

    return true;
}

bool CFG_factoryGetCertificate(char* o_pStr)
{
    cJSON*  object;

    object = _getFactoryObject("certificate");
    if (!object) {
        return false;
    }

    return true;
}

bool CFG_factoryGetManufacturingDate(char* o_pStr)
{
    cJSON*  object;

    object = _getFactoryObject("manufacturing_date");
    if (!object) {
        return false;
    }

    return true;
}

bool CFG_factoryGetSn(char* o_pStr)
{
    cJSON*  object;

    object = _getFactoryObject("sn");
    if (!object) {
        return false;
    }

    return true;
}

bool CFG_factoryGetHwRevision(char* o_pStr)
{
    cJSON*  object;

    object = _getFactoryObject("hw_revision");
    if (!object) {
        return false;
    }

    return true;
}

bool CFG_factoryGetModel(char* o_pStr)
{
    cJSON*  object;

    object = _getFactoryObject("model");
    if (!object) {
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

static bool dbgJson(uint8_t argc, char** argv)
{
    cJSON *json = NULL;
    const cJSON *object = NULL;

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
        PRINT("string:\n");
        PRINT("string: %s\n", object->valuestring);

    }

    if (cJSON_IsNumber(object)) {
        PRINT("number:\n");
        PRINT("number: %f\n", object->valuedouble);
    }

    return true;
}


static bool dbgGetObject(uint8_t argc, char** argv)
{
    static cJSON* object;
    if (argc < 2) {
        return false;
    }

    object = _getFactoryObject(argv[1]);
    if (!object) {
        PRINT("failed to get object\n");
        return true;
    }

    PRINT("%s\n", object->valuestring);

    return true;
}

static bool dbgFreeObject(uint8_t argc, char** argv)
{
    _freeObject();
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
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*


void CFG_init(void)
{
    DBG_TREE_add("/",		g_menu);
}