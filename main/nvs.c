
#define DEF_DBG_MODULE	DBG_MODULE_NVS

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include <errno.h>
#include <nvs_flash.h>
#include "main.h"
#include "nvs.h"

static struct {
	nvs_handle_t nvsHandle;
} g_nvs;

bool NVS_get(char* namespace, char* key,  char* val, size_t maxSize)
{
	bool    ret = true;
	esp_err_t err = ESP_OK;
	nvs_handle_t handle;
	size_t length = maxSize;

	if (!namespace) {
		return false;
	}

	if (!key) {
		return false;
	}

	err = nvs_open(namespace, NVS_READONLY, &handle);
	if (err != ESP_OK) {
		ERROR("nvs_open <%s> failed %s\n", namespace, ESP_getErrStr(err));
		return false;
	}

	err =  nvs_get_str(handle, key, val, &length);
	if (err != ESP_OK) {
		TRACE("nvs_get_str <%s,%s> failed 0x%X\n", namespace, key, err);
		ret = false;
		goto exit;
	}
	val[length] = '\0';

exit:
	nvs_close(handle);

	return ret;
}

bool NVS_set(char* namespace, char* key,  char* val)
{
	bool    ret = true;
	esp_err_t err = ESP_OK;
	nvs_handle_t handle;

	char*	rdStr;
	size_t  length;

	err = nvs_open(namespace, NVS_READWRITE, &handle);
	if (err != ESP_OK) {
		ERROR("nvs_open failed\n");
		return false;
	}

	length = 4096;
	rdStr = calloc(1, length);
	if (!rdStr) {
		WARN("can't allocate rdStr. storing\n");
		goto store;
	}

	err =  nvs_get_str(handle, key, rdStr, &length);
	if (err != ESP_OK) {
		TRACE("nvs_get_str <%s> failed\n", key);
		goto    store;
	}

	rdStr[length] = '\0';
	if (strcmp(rdStr, val)) {
		INFO("<%s> mismatch. storing new <%s>\n", key, val);
		goto store;
	}

	INFO("no need to store <%s>\n", key);
	goto exit;

store:
	INFO("storing %s\n", key);
	err = nvs_set_str(handle, key, val);
	if (err != ESP_OK) {
		ERROR("nvs_set_str failed %s(len:%d) %s\n", key, strlen(val), ESP_getErrStr(err));
		ret = false;
	}

exit:
	if (rdStr) {
		free(rdStr);
	}

	nvs_commit(handle);
	nvs_close(handle);

	return ret;
}

bool NVS_del(char* namespace, char* key)
{
	bool    ret = true;
	esp_err_t err = ESP_OK;
	nvs_handle_t handle;

	err = nvs_open(namespace, NVS_READWRITE, &handle);
	if (err != ESP_OK) {
		ERROR("nvs_open failed\n");
		return false;
	}

	INFO("deleting %s\n", key);
	err = nvs_erase_key(handle, key);
	if (err != ESP_OK) {
		ERROR("nvs_erase_key %s %s\n", key, ESP_getErrStr(err));
		ret = false;
	}

	nvs_commit(handle);
	nvs_close(handle);

	return ret;
}

bool NVS_eraseNamespace(char* ns)
{
	esp_err_t err = ESP_OK;
	nvs_handle_t handle;

	err = nvs_open(ns, NVS_READWRITE, &handle);
	if (err != ESP_OK) {
		ERROR("nvs_open <%s> failed %x\n",  NVS_NAMESPACE, err);
		return false;
	}

	err = nvs_erase_all(handle);
	if (err != ESP_OK) {
		ERROR("nvs_erase_all failed %s\n",  ESP_getErrStr(err));
		return false;
	}

	return true;
}

static bool dbgOpen(uint8_t argc, char** argv)
{
	esp_err_t   err = ESP_OK;

	if (argc < 2) {
		return false;
	} else {
		err = nvs_open(argv[1], NVS_READWRITE, &g_nvs.nvsHandle);
	}

	if (ESP_OK != err) {
		ERROR("nvs_open %s\n", ESP_getErrStr(err));
	}

	return true;
}

static bool dbgClose(uint8_t argc, char** argv)
{
	nvs_close(g_nvs.nvsHandle);
	g_nvs.nvsHandle = (nvs_handle_t)NULL;

	return true;
}

static bool dbgCommit(uint8_t argc, char** argv)
{
	esp_err_t   err = ESP_OK;

	err = nvs_commit(g_nvs.nvsHandle);
	if (ESP_OK != err) {
		ERROR("nvs_commit %s\n", ESP_getErrStr(err));
	}

	return true;
}

static bool dbgGet(uint8_t argc, char** argv)
{
	esp_err_t   err = ESP_OK;
	char str[2048];
	size_t length = sizeof(str);

	if (argc < 2) {
		return false;
	}

	err =  nvs_get_str(g_nvs.nvsHandle, argv[1], str, &length);
	if (ESP_OK != err) {
		ERROR("nvs_get_str %s\n", ESP_getErrStr(err));
		return true;
	}

	str[length] = '\0';
	PRINT("str=<%s>\n", str);

	return true;
}

static bool dbgSet(uint8_t argc, char** argv)
{
	esp_err_t   err = ESP_OK;

	if (argc < 3) {
		return false;
	}

	err = nvs_set_str(g_nvs.nvsHandle, argv[1], argv[2]);
	if (ESP_OK != err) {
		ERROR("nvs_set_str %s\n", ESP_getErrStr(err));
	}

	return true;
}

static bool dbgErase(uint8_t argc, char** argv)
{
	esp_err_t   err = ESP_OK;

	if (argc < 2) {
		return false;
	}

	err = nvs_erase_key(g_nvs.nvsHandle, argv[1]);
	if (ESP_OK != err) {
		ERROR("nvs_erase_key %s\n", ESP_getErrStr(err));
	}

	return true;
}

static bool dbgList(uint8_t argc, char** argv)
{
	esp_err_t   err = ESP_FAIL;
	nvs_iterator_t it;
	nvs_handle_t handle;

	if (argc >= 2) {
		err = nvs_open(argv[1], NVS_READONLY, &handle);
		if (err == ESP_OK) {
			err = nvs_entry_find_in_handle(handle, NVS_TYPE_ANY, &it);
		}
	} else {
		err =  nvs_entry_find(NVS_DEFAULT_PART_NAME, NULL, NVS_TYPE_ANY, &it);
	}

	while (err == ESP_OK) {
		nvs_entry_info_t info;
		char* pTypeStr = "";

		nvs_entry_info(it, &info); // Can omit error check if parameters are guaranteed to be non-NULL
		PRINT("'%s', key: '%s', type: '%x'", info.namespace_name, info.key, info.type);

		switch (info.type) {
			case NVS_TYPE_U8:
				pTypeStr = "U8";
				break;
			case NVS_TYPE_I8:
				pTypeStr = "I8";
				break;
			case NVS_TYPE_U16:
				pTypeStr = "U16";
				break;
			case NVS_TYPE_I16:
				pTypeStr = "I16";
				break;
			case NVS_TYPE_U32:
				pTypeStr = "U32";
				break;
			case NVS_TYPE_I32:
				pTypeStr = "I32";
				break;
			case NVS_TYPE_U64:
				pTypeStr = "U64";
				break;
			case NVS_TYPE_I64:
				pTypeStr = "I64";
				break;
			case NVS_TYPE_STR:
				pTypeStr = "STR";
				break;
			case NVS_TYPE_BLOB:
				pTypeStr = "BLOB";
				break;
			default:
		}
		PRINT("%s\n", pTypeStr);
		err = nvs_entry_next(&it);
	}

	if (handle) {
		nvs_close(handle);
	}

	return true;
}

static bool dbgStatus(uint8_t argc, char** argv)
{
	bool		ret;
	esp_err_t   err = ESP_OK;
	nvs_stats_t nvs_stats;
	uint8_t		i;
	char		val[2048];

	err =  nvs_get_stats(NULL, &nvs_stats);
	if (err == ESP_OK) {
		PRINT("used_entries   : %d\n", nvs_stats.used_entries);
		PRINT("free_entries   : %d\n", nvs_stats.free_entries);
		PRINT("total_entries  : %d\n", nvs_stats.total_entries);
		PRINT("namespace_count: %d\n", nvs_stats.namespace_count);
	}

	return true;
}

// *INDENT-OFF*
DEBUG_MENU_START(g_menu)
	DEBUG_MENU_DIR("nvs", NULL)
		DEBUG_MENU_CMD("status",    NULL,		NULL, dbgStatus)
		DEBUG_MENU_CMD("open",	    NULL,		NULL, dbgOpen)
		DEBUG_MENU_CMD("close",	    NULL,		NULL, dbgClose)
		DEBUG_MENU_CMD("commit",    NULL,		NULL, dbgCommit)
		DEBUG_MENU_CMD("get",       NULL,		NULL, dbgGet)
		DEBUG_MENU_CMD("set",       NULL,		NULL, dbgSet)
		DEBUG_MENU_CMD("erase",		NULL,		NULL, dbgErase)
		DEBUG_MENU_CMD("list",      NULL,		NULL, dbgList)
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*


void NVS_init(void)
{
	DBG_TREE_add("/",		g_menu);
}