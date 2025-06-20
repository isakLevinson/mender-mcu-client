
#define DEF_DBG_MODULE	DBG_MODULE_NVS

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include <errno.h>
#include <nvs_flash.h>
#include "main.h"
#include "nvs.h"

#define	NVS_NAMESPACE      "cfg"

static struct {
	nvs_handle_t nvsHandle;
} g_nvs;

nvs_arr_t g_id[] = {
	NVS_LIST(NVS_ARR)
	{.pId = NULL, .pDefault = NULL}
};

static bool _get(char* key,  char* val)
{
	bool    ret = true;
	esp_err_t err = ESP_OK;
	nvs_handle_t handle;
	size_t length = NVS_MAX_LENGTH;

	err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
	if (err != ESP_OK) {
		ERROR("nvs_open <%s> failed %x\n",  NVS_NAMESPACE, err);
		ESP_printErr(err);
		return false;
	}

	err =  nvs_get_str(handle, key, val, &length);
	if (err != ESP_OK) {
		TRACE("nvs_get_str <%s> failed 0x%X\n", key, err);
		ret = false;
		goto exit;
	}
	val[length] = '\0';

exit:
	nvs_close(handle);
	//ESP_printErr(err);

	return ret;
}

static bool _set(char* key,  char* val)
{
	bool    ret = true;
	esp_err_t err = ESP_OK;
	nvs_handle_t handle;
	char    str[NVS_MAX_LENGTH];
	size_t  length;

	err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
	if (err != ESP_OK) {
		ERROR("nvs_open failed\n");
		return false;
	}

	length = sizeof(str);
	err =  nvs_get_str(handle, key, str, &length);
	if (err != ESP_OK) {
		TRACE("nvs_get_str <%s> failed\n", key);
		goto    store;
	}
	str[length] = '\0';
	if (strcmp(str, val)) {
		INFO("<%s> mismatch. storing new <%s>\n", key, val);
		goto store;
	}

	INFO("no need to store <%s>\n", key);
	goto exit;

store:
	INFO("storing %s\n", key);
	err = nvs_set_str(handle, key, val);
	if (err != ESP_OK) {
		ERROR("nvs_set_str ssid failed %x\n", err);
		ret = false;
	}

exit:
	nvs_close(handle);
	return ret;
}

bool NVS_isValidName(char* pName)
{
	uint8_t	i= 1 ; // first one is "INVALID"

	while (g_id[i].pId) {
		if (!strcmp(g_id[i].pId, pName)) {
			return true;
		}
		i++;
	}

	return false;
}

bool NVS_get(nvs_id_t id,  char* val)
{
	bool	ret;

	ret = _get(g_id[id].pId, val);
	if (!ret) {
		TRACE("get failed id %d. using default value\n", id);
		if (g_id[id].pDefault) {
			strcpy(val, g_id[id].pDefault);
			return true;
		} else {
			return false;
		}
	}

	return true;
}

bool NVS_set(nvs_id_t id,  char* val)
{
	bool	ret;

	ret = _set(g_id[id].pId, val);

	return ret;
}

bool NVS_get_ssid(char* ssid, char* passwd)
{
	bool    ret = true;

	ret = NVS_get(nvs_id_ssid, ssid);
	if (!ret)  {
		ERROR("get ssid failed\n");
		return false;
	}

	if (ssid[0] == '\0') {
		return false;
	}

	ret = NVS_get(nvs_id_passwd, passwd);
	if (!ret)  {
		ERROR("get passwd failed\n");
		return false;
	}

	if (passwd[0] == '\0') {
		return false;
	}

	return true;
}

bool NVS_set_ssid(char* ssid, char* passwd)
{
	bool    ret = true;

	ret = NVS_set(nvs_id_ssid, ssid);
	if (!ret)  {
		ERROR("set ssid failed\n");
	}
	ret &= NVS_set(nvs_id_passwd, passwd);
	if (!ret)  {
		ERROR("set passwd failed\n");
	}

	return ret;
}

bool NVS_eraseAll(void)
{
	esp_err_t err = ESP_OK;
	nvs_handle_t handle;

	err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
	if (err != ESP_OK) {
		ERROR("nvs_open <%s> failed %x\n",  NVS_NAMESPACE, err);
		return false;
	}

	err = nvs_erase_all(handle);
	if (err != ESP_OK) {
		ERROR("nvs_erase_all failed %x\n",  err);
		ESP_printErr(err);
		return false;
	}

	return true;
}

static bool dbgOpen(uint8_t argc, char** argv)
{
	esp_err_t   err = ESP_OK;

	if (argc < 2) {
		err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &g_nvs.nvsHandle);
	} else {
		err = nvs_open(argv[1], NVS_READWRITE, &g_nvs.nvsHandle);
	}

	ESP_printErr(err);

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
	ESP_printErr(err);

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
	ESP_printErr(err);

	if (err == ESP_OK) {
		str[length] = '\0';
		PRINT("str=<%s>\n", str);
	}

	return true;
}

static bool dbgSet(uint8_t argc, char** argv)
{
	esp_err_t   err = ESP_OK;

	if (argc < 3) {
		return false;
	}

	err = nvs_set_str(g_nvs.nvsHandle, argv[1], argv[2]);
	ESP_printErr(err);

	return true;
}

static bool dbgList(uint8_t argc, char** argv)
{
	esp_err_t   err = ESP_OK;
	nvs_iterator_t it;

	err =  nvs_entry_find(NVS_DEFAULT_PART_NAME, NULL, NVS_TYPE_ANY, &it);
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

	return true;
}

static bool dbgId(uint8_t argc, char** argv)
{
	bool	ret;
	char	buf[2048];
	uint8_t	id;

	if (argc < 2) {
		for (id=1; id<nvs_id_last; id++) {
			ret = NVS_get(id,  buf);
			PRINT("%d %s: %s\n", id, g_id[id].pId, buf);
		}
		return false;
	}

	id = strtol(argv[1], NULL, 10);
	if (argc < 3) {
		ret = NVS_get(id,  buf);
		PRINT("%s\n", buf);
		return true;
	}

	ret = NVS_set(id,  argv[2]);

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
		DEBUG_MENU_CMD("list",      NULL,		NULL, dbgList)
		DEBUG_MENU_CMD("id",	    NULL,		NULL, dbgId)
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*


void NVS_init(void)
{
	DBG_TREE_add("/",		g_menu);
}