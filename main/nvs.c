
#define DEF_DBG_MODULE	DBG_MODULE_ADC

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include <errno.h>
#include <nvs_flash.h>

#define   WIFI_MAX_SSID_LENGTH    32
#define   WIFI_MAX_PASSWD_LENGTH  32


static struct {
    nvs_handle_t nvsHandle;
} g_nvs;

bool NVS_get_ssid(char* ssid, char* passwd)
{
    bool    ret = true;
    esp_err_t err = ESP_OK;
    nvs_handle_t handle;
    size_t length;
    char* pStr = NULL;

    err = nvs_open(NVS_NAMESPACE_WIFI, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        printf("nvs_open <%s> failed %x\n\n",  NVS_NAMESPACE_WIFI, err);
        return false;
    }

    length = WIFI_MAX_SSID_LENGTH;
    err =  nvs_get_str(handle, NVS_KEY_WIFI_SSID, ssid, &length);
    if (err != ESP_OK) {
        printf("nvs_get_str ssid failed\n\n");
        ret = false;
        goto exit;
    }
    ssid[length] = '\0';

    length = WIFI_MAX_PASSWD_LENGTH;
    err =  nvs_get_str(handle, NVS_KEY_WIFI_PASSWD, passwd, &length);
    if (err != ESP_OK) {
        printf("nvs_get_str passwd failed\n\n");
        ret = false;
        goto exit;
    }
    passwd[length] = '\0';

    exit:
    nvs_close(handle);

    switch(err) {
    case ESP_OK:    break;
    case ESP_ERR_NVS_NOT_FOUND:         pStr = "ESP_ERR_NVS_NOT_FOUND"; break;
    case ESP_ERR_NVS_NOT_INITIALIZED:   pStr = "ESP_ERR_NVS_NOT_INITIALIZED";  break;
    case ESP_ERR_NO_MEM:                pStr = "ESP_ERR_NO_MEM";  break;
    case ESP_ERR_INVALID_ARG:           pStr = "ESP_ERR_INVALID_ARG";  break;
    default:
    }

    if (pStr) {
        printf("%s\n\n", pStr);
    } else {
        if (ESP_OK != err) {
            printf("0x%x\n", err);
        }
    }

    return ret;
}

bool NVS_set_ssid(char* ssid, char* passwd)
{
    bool    ret = true;
    esp_err_t err = ESP_OK;
    nvs_handle_t handle;
    char    str[WIFI_MAX_SSID_LENGTH];
    size_t  length;

    err = nvs_open(NVS_NAMESPACE_WIFI, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ERROR("nvs_open failed\n");
        return false;
    }

    length = WIFI_MAX_SSID_LENGTH;
    err =  nvs_get_str(handle, NVS_KEY_WIFI_SSID, str, &length);
    if (err != ESP_OK) {
        WARN("nvs_get_str ssid failed\n");
        goto    store;
    }
    str[length] = '\0';
    if (strcmp(str, ssid)) {
        INFO("ssid mismatch. storing new <%s> <%s>\n", ssid, passwd);
        goto store;
    }
    length = WIFI_MAX_PASSWD_LENGTH;
    err =  nvs_get_str(handle, NVS_KEY_WIFI_PASSWD, str, &length);
    if (err != ESP_OK) {
        WARN("nvs_get_str passwd failed\n");
        goto    store;
    }
    str[length] = '\0';
    if (strcmp(str, passwd)) {
        INFO("passwd mismatch. storing new <%s> <%s>\n", ssid, passwd);
        goto store;
    }

    INFO("no need to store ssid or passwd\n");
    goto exit;

    store:
        err = nvs_set_str (handle, NVS_KEY_WIFI_SSID, ssid);
        if (err != ESP_OK) {
            printf("nvs_set_str ssid failed %x\n", err);
            ret = false;
            goto exit;
        }

        err = nvs_set_str (handle, NVS_KEY_WIFI_PASSWD, passwd);
        if (err != ESP_OK) {
            printf("nvs_set_str passwd failed %x\n", err);
            ret = false;
            goto exit;
        }


    exit:
        nvs_close(handle);
        return ret;
}

static bool dbgNvs(uint8_t argc, char **argv)
{
    bool        ret;
    esp_err_t   err = ESP_OK;
    bool    close   = false;
    bool    commit  = false;
    bool    status  = false;
    bool    list    = false;
    char*   openStr = NULL;
    char*   getKey  = NULL;
    char*   setKey  = NULL;
    char*   arg1    = NULL;
    char*   arg2    = NULL;

// *INDENT-OFF*
	ARGS_ENTRY_BEGIN(args)
		ARGS_ENTRY("open",		ARGS_TYPE_STRING,		0,	"",		&openStr)
		ARGS_ENTRY("close",		ARGS_TYPE_SWITCH,		0,	"",		&close)
		ARGS_ENTRY("commit",	ARGS_TYPE_SWITCH,		0,	"",		&commit)
		ARGS_ENTRY("status",	ARGS_TYPE_SWITCH,		0,	"",		&status)
		ARGS_ENTRY("list",	    ARGS_TYPE_SWITCH,		0,	"",		&list)
        ARGS_ENTRY("get",       ARGS_TYPE_STRING,		0,	"", 	&getKey)
        ARGS_ENTRY("set",       ARGS_TYPE_STRING,		0,	"", 	&setKey)
        ARGS_ENTRY(NULL,        ARGS_TYPE_STRING,		0,	"arg1",	&arg1)
        ARGS_ENTRY(NULL,        ARGS_TYPE_STRING,		0,	"arg2",	&arg2)
	ARGS_ENTRY_END()
// *INDENT-ON*

	ret = ARGS_readValues(argc, argv, args, NULL, NULL);
	if (!ret) {
		return false;
	}

    if (status) {
        nvs_stats_t nvs_stats;

        printf("stats\n\n");
        err =  nvs_get_stats(NULL, &nvs_stats);
        if (err == ESP_OK) {
            printf("used_entries   : %d\n", nvs_stats.used_entries);
            printf("free_entries   : %d\n", nvs_stats.free_entries);
            printf("total_entries  : %d\n", nvs_stats.total_entries);
            printf("namespace_count: %d\n", nvs_stats.namespace_count);
        }
    } else if (list) {
        nvs_iterator_t it;
        printf("list\n\n");
        
        err =  nvs_entry_find(NVS_DEFAULT_PART_NAME, NULL, NVS_TYPE_ANY, &it);
        while (err == ESP_OK) {
            nvs_entry_info_t info;
            char* pTypeStr = "";

            nvs_entry_info(it, &info); // Can omit error check if parameters are guaranteed to be non-NULL
            PRINT("'%s', key: '%s', type: '%x'", info.namespace_name, info.key, info.type);

            switch (info.type) {
                case NVS_TYPE_U8:   pTypeStr = "U8";  break;
                case NVS_TYPE_I8:   pTypeStr = "I8";  break;
                case NVS_TYPE_U16:  pTypeStr = "U16";  break;
                case NVS_TYPE_I16:  pTypeStr = "I16";  break;
                case NVS_TYPE_U32:  pTypeStr = "U32";  break;
                case NVS_TYPE_I32:  pTypeStr = "I32";  break;
                case NVS_TYPE_U64:  pTypeStr = "U64";  break;
                case NVS_TYPE_I64:  pTypeStr = "I64";  break;
                case NVS_TYPE_STR:  pTypeStr = "STR";  break;
                case NVS_TYPE_BLOB: pTypeStr = "BLOB";  break;
                default:
            }
            PRINT("%s\n", pTypeStr);
            err = nvs_entry_next(&it);
        }
    } else if (commit) {
        err = nvs_commit(g_nvs.nvsHandle);
    } else if (close) {
        nvs_close(g_nvs.nvsHandle);
        g_nvs.nvsHandle = (nvs_handle_t)NULL;
    }

    if (openStr) {
        PRINT("opening %s\n", openStr);
        err = nvs_open(openStr, NVS_READWRITE, &g_nvs.nvsHandle);
    }

    if (getKey) {
        char str[32];
        size_t length = sizeof(str);
        PRINT("get %s\n", getKey);
        err =  nvs_get_str(g_nvs.nvsHandle, getKey, str, &length);
        if (err == ESP_OK) {
            str[length] = '\0';
            PRINT("str=<%s>\n", str);
        }
    }

    if (setKey) {
        err = nvs_set_str(g_nvs.nvsHandle, setKey, arg1);
    }

    if (err != ESP_OK) {
        char* pStr = NULL;

        switch(err) {
        case ESP_ERR_NVS_NOT_FOUND:         pStr = "ESP_ERR_NVS_NOT_FOUND"; break;
        case ESP_ERR_NVS_NOT_INITIALIZED:   pStr = "ESP_ERR_NVS_NOT_INITIALIZED";  break;
        case ESP_ERR_NO_MEM:                pStr = "ESP_ERR_NO_MEM";  break;
        case ESP_ERR_INVALID_ARG:           pStr = "ESP_ERR_INVALID_ARG";  break;
        case ESP_ERR_NVS_INVALID_LENGTH:    pStr = "ESP_ERR_NVS_INVALID_LENGTH";   break;
        }

        if (pStr) {
            PRINT("failed %s\n\n", pStr);
        } else {
            PRINT("failed 0x%x\n\n", err);
        }
    }

    return true;
}

// *INDENT-OFF*
DEBUG_MENU_START(g_menu)
	DEBUG_MENU_DIR("nvs", NULL)
		DEBUG_MENU_CMD("nvs",	        NULL,		NULL, dbgNvs)
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*


void NVS_init(void)
{
    DBG_TREE_add("/",		g_menu);
}