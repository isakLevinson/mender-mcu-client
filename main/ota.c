
#define DEF_DBG_MODULE	DBG_MODULE_OTA

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include "esp_http_client.h"
#include "esp_https_ota.h"

extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");


esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        INFO("HTTP_EVENT_ERROR\n");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        INFO("HTTP_EVENT_ON_CONNECTED\n");
        break;
    case HTTP_EVENT_HEADER_SENT:
        INFO("HTTP_EVENT_HEADER_SENT\n");
        break;
    case HTTP_EVENT_ON_HEADER:
        INFO("HTTP_EVENT_ON_HEADER, key=%s, value=%s\n", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        TRACE("HTTP_EVENT_ON_DATA, len=%d\n", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        INFO("HTTP_EVENT_ON_FINISH\n");
        break;
    case HTTP_EVENT_DISCONNECTED:
        INFO("HTTP_EVENT_DISCONNECTED\n");
        break;
    case HTTP_EVENT_REDIRECT:
        INFO("HTTP_EVENT_REDIRECT\n");
        break;
    }
    return ESP_OK;
}


static bool dbgDownload(uint8_t argc, char** argv)
{
    if (argc < 2) {
        return false;
    }

    esp_http_client_config_t config = {
#ifdef CONFIG_EXAMPLE_USE_CERT_BUNDLE
        .crt_bundle_attach = esp_crt_bundle_attach,
#else
        .cert_pem = (char *)server_cert_pem_start,
#endif /* CONFIG_EXAMPLE_USE_CERT_BUNDLE */
        .event_handler = _http_event_handler,
        .keep_alive_enable = true,
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

    config.url = argv[1];

    INFO("Attempting to download update from %s\n", config.url);
    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        INFO("OTA Succeed\n");
    } else {
        ERROR("Firmware upgrade failed %d\n", ret);
    }

    return true;
}


static bool dbgRestart(uint8_t argc, char** argv)
{
    esp_restart();
    return true;
}

static bool dbgStatus(uint8_t argc, char** argv)
{
     return true;
}

DEBUG_MENU_START(g_menu)
    DEBUG_MENU_DIR("ota", NULL)
	    DEBUG_MENU_CMD("status",			NULL,		NULL, dbgStatus)
	    DEBUG_MENU_CMD("download",			NULL,		NULL, dbgDownload)
	    DEBUG_MENU_CMD("restart",			NULL,		NULL, dbgRestart)
    DEBUG_MENU_DIR_END
DEBUG_MENU_END


void OTA_init(void)
{
    DBG_TREE_add("/",		g_menu);
}