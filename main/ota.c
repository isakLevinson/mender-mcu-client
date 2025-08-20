
#define DEF_DBG_MODULE	DBG_MODULE_OTA

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "config.h"

static struct {
	esp_https_ota_handle_t  handle;
} g_ota;

esp_err_t _http_event_handler(esp_http_client_event_t* evt)
{
	TRACE("_http_event_handler: ");

	switch (evt->event_id) {
		case HTTP_EVENT_ERROR:
			TRACE("HTTP_EVENT_ERROR");
			break;
		case HTTP_EVENT_ON_CONNECTED:
			TRACE("HTTP_EVENT_ON_CONNECTED");
			break;
		case HTTP_EVENT_HEADER_SENT:
			TRACE("HTTP_EVENT_HEADER_SENT");
			break;
		case HTTP_EVENT_ON_HEADER:
			TRACE("HTTP_EVENT_ON_HEADER");
			TRACE_BUF("key", PRINT_BUF_STYLE_ASC_SIZE_NL, evt->header_key, 10);
			TRACE_BUF("val", PRINT_BUF_STYLE_ASC_SIZE_NL, evt->header_value, 10);
			break;
		case HTTP_EVENT_ON_DATA:
			TRACE("HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
			break;
		case HTTP_EVENT_ON_FINISH:
			TRACE("HTTP_EVENT_ON_FINISH");
			break;
		case HTTP_EVENT_DISCONNECTED:
			TRACE("HTTP_EVENT_DISCONNECTED");
			break;
		case HTTP_EVENT_REDIRECT:
			TRACE("HTTP_EVENT_REDIRECT");
			break;
		default:
			TRACE("%d", evt->event_id);
	}

	TRACE("\n");

	return ESP_OK;
}

static bool _configInit(esp_http_client_config_t* cfg)
{
	bool	ret;

	cfg->cert_pem = calloc(1, 2048);
	if (!cfg->cert_pem) {
		return false;
	}

	cfg->client_cert_pem = calloc(1, 4048);
	if (!cfg->client_cert_pem) {
		return false;
	}

	cfg->client_key_pem = calloc(1, 2048);
	if (!cfg->client_key_pem) {
		return false;
	}

	ret = CFG_get(cfg_id_ca_pem, (char*)cfg->cert_pem, 2048);
	if (!ret) {
		return false;
	}

	ret = CFG_get(cfg_id_cert_pem, (char*)cfg->client_cert_pem, 4096);
	if (!ret) {
		return false;
	}

	ret = CFG_get(cfg_id_cert_key, (char*)cfg->client_key_pem, 2048);
	if (!ret) {
		return false;
	}

	return true;
}

static bool _configFree(esp_http_client_config_t* cfg)
{
	if (cfg->cert_pem) {
		free((void*)cfg->cert_pem);
	}
	
	if (cfg->client_cert_pem) {
		free((void*)cfg->client_cert_pem);
	}

	if (cfg->client_key_pem) {
		free((void*)cfg->client_key_pem);
	}

	return true;
}


bool OTA_auto(char* pUrl)
{
	bool	ret = true;
	esp_http_client_config_t config = {
		.url = pUrl,
		.event_handler = _http_event_handler,
		.keep_alive_enable = true,
		.skip_cert_common_name_check = true,
#ifdef CONFIG_EXAMPLE_FIRMWARE_UPGRADE_BIND_IF
		.if_name = &ifr,
#endif
	};

#ifdef CONFIG_EXAMPLE_SKIP_COMMON_NAME_CHECK
	config.skip_cert_common_name_check = true;
#endif

	esp_https_ota_config_t ota_config = {
		.http_config = &config,
	};

	ret = _configInit(&config);
	if (!ret) {
		goto exit;
	}

	INFO("Attempting to download update from %s\n", config.url);
	esp_err_t status = esp_https_ota(&ota_config);
	if (status == ESP_OK) {
		INFO("OTA Succeed\n");
	} else {
		ERROR("Firmware upgrade failed %d\n", ret);
		ret = false;
		goto exit;
	}

exit:
	_configFree(&config);
	return ret;
}

bool OTA_begin(char* pUrl)
{
	bool		ret = true;
	esp_err_t   err;

	esp_http_client_config_t config = {
		.url = pUrl,
		.event_handler = _http_event_handler,
		.keep_alive_enable = true,
		.skip_cert_common_name_check = true,
	};

	esp_https_ota_config_t ota_config = {
		.http_config = &config,
	};
	ret = _configInit(&config);
	if (!ret) {
		goto exit;
	}

	INFO("begin OTA from %s\n", config.url);

	err = esp_https_ota_begin(&ota_config, &g_ota.handle);
	if (ESP_OK != err) {
		ERROR("esp_https_ota_begin failed %d\n", err);
		ret = false;
		goto exit;
	}

exit:
	_configFree(&config);
	return ret;
}

bool OTA_perform(void)
{
	esp_err_t   err;
	int         size;
	bool		complete;

	do {
		err = esp_https_ota_perform(g_ota.handle);
		size = esp_https_ota_get_image_len_read(g_ota.handle);
		complete = esp_https_ota_is_complete_data_received(g_ota.handle);
		INFO("read: %d %d\n", size, complete);
	} while (ESP_ERR_HTTPS_OTA_IN_PROGRESS == err);

	if (ESP_OK != err) {
		ERROR("esp_https_ota_perform failed %d\n", err);
		return false;
	}

	err =  esp_https_ota_finish(g_ota.handle);
	if (ESP_OK != err) {
		ERROR("esp_https_ota_finish failed %d\n", err);
		return false;
	}

	return true;
}

void OTA_restart(void)
{
	esp_restart();
}

static bool dbgAuto(uint8_t argc, char** argv)
{
//	char    url[64];

	if (argc < 2) {
		return false;
	}

	//	sprintf(url, "https://%s:8070/%s", argv[1], argv[2]);
	//	config.url = url;

	OTA_auto(argv[1]);

	return true;
}

static bool dbgBegin(uint8_t argc, char** argv)
{
	if (argc < 2) {
		return false;
	}

	OTA_begin(argv[1]);

	return true;
}

static bool dbgPerform(uint8_t argc, char** argv)
{
	OTA_perform();

	return true;
}

static bool dbgFinish(uint8_t argc, char** argv)
{
	esp_err_t   err;

	err =  esp_https_ota_finish(g_ota.handle);
	if (ESP_OK != err) {
		ERROR("esp_https_ota_finish failed %d\n", err);
	}

	return true;
}

static bool dbgAbort(uint8_t argc, char** argv)
{
	esp_err_t   err;

	err =  esp_https_ota_abort(g_ota.handle);
	if (ESP_OK != err) {
		ERROR("esp_https_ota_abort failed %d\n", err);
	}

	return true;
}

static bool dbgRestart(uint8_t argc, char** argv)
{
	OTA_restart();
	return true;
}

static bool dbgStatus(uint8_t argc, char** argv)
{
	esp_err_t       err;
	int             size;
	esp_app_desc_t  new_app_info;

	err = esp_https_ota_get_status_code(g_ota.handle);
	if (err < 0) {
		ERROR("esp_https_ota_get_status_code failed %d\n", err);
	} else {
		PRINT("esp_https_ota_get_status_code %d\n", err);
	}

	size = esp_https_ota_get_image_size(g_ota.handle);
	PRINT("image size: %d\n", size);

	err = esp_https_ota_get_img_desc(g_ota.handle, &new_app_info);
	if (ESP_OK != err) {
		ERROR("esp_https_ota_get_img_desc failed %d\n", err);
	} else {
		PRINT("magic_word     : 0x%x\n", new_app_info.magic_word);
		PRINT("secure_version : %d\n", new_app_info.secure_version);
		PRINT_BUF("ver", PRINT_BUF_STYLE_ASC_SIZE_NL, new_app_info.version, sizeof(new_app_info.version));
		PRINT_BUF("proj", PRINT_BUF_STYLE_ASC_SIZE_NL, new_app_info.project_name, sizeof(new_app_info.project_name));
		PRINT_BUF("time", PRINT_BUF_STYLE_ASC_SIZE_NL, new_app_info.time, sizeof(new_app_info.time));
		PRINT_BUF("date", PRINT_BUF_STYLE_ASC_SIZE_NL, new_app_info.date, sizeof(new_app_info.date));
		PRINT_BUF("idf", PRINT_BUF_STYLE_ASC_SIZE_NL, new_app_info.idf_ver, sizeof(new_app_info.idf_ver));
	}

	return true;
}

// *INDENT-OFF*
DEBUG_MENU_START(g_menu)
	DEBUG_MENU_DIR("ota", NULL)
		DEBUG_MENU_CMD("status",	NULL,		    NULL, dbgStatus)
		DEBUG_MENU_CMD("auto",		NULL,		    NULL, dbgAuto)
		DEBUG_MENU_CMD("restart",	NULL,		    NULL, dbgRestart)
		DEBUG_MENU_CMD("begin",		"<ip> <file>",	NULL, dbgBegin)
		DEBUG_MENU_CMD("perform",	NULL,		    NULL, dbgPerform)
		DEBUG_MENU_CMD("finish",	NULL,		    NULL, dbgFinish)
		DEBUG_MENU_CMD("abort", 	NULL,		    NULL, dbgAbort)
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*

void OTA_init(void)
{
	DBG_TREE_add("/",		g_menu);
}
