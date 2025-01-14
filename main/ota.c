
#define DEF_DBG_MODULE	DBG_MODULE_OTA

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include <stdio.h>

#include <sys/stat.h>
#include "sdkconfig.h"

#include <regex.h>
#include <time.h>
#include <nvs_flash.h>
#include <esp_mac.h>
#include <esp_ota_ops.h>
#include <esp_event.h>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "mender-client.h"
#include "mender-configure.h"
#include "mender-flash.h"
#include "mender-inventory.h"
#include "mender-troubleshoot.h"
#include <protocol_examples_common.h>

extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

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

int OTA_auto(char* pUrl)
{
	\
	esp_http_client_config_t config = {
		.url = pUrl,
#ifdef CONFIG_EXAMPLE_USE_CERT_BUNDLE
		.crt_bundle_attach = esp_crt_bundle_attach,
#else
		.cert_pem = (char*)server_cert_pem_start,
#endif /* CONFIG_EXAMPLE_USE_CERT_BUNDLE */
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

	INFO("Attempting to download update from %s\n", config.url);
	esp_err_t ret = esp_https_ota(&ota_config);
	if (ret == ESP_OK) {
		INFO("OTA Succeed\n");
	} else {
		ERROR("Firmware upgrade failed %d\n", ret);
	}

	return ret;
}

bool OTA_begin(char* pUrl)
{
	esp_err_t   err;

	esp_http_client_config_t config = {
		.url = pUrl,
		.cert_pem = (char*)server_cert_pem_start,
		.event_handler = _http_event_handler,
		.keep_alive_enable = true,
		.skip_cert_common_name_check = true,
	};

	esp_https_ota_config_t ota_config = {
		.http_config = &config,
	};

	INFO("begin OTA from %s\n", config.url);

	err = esp_https_ota_begin(&ota_config, &g_ota.handle);
	if (ESP_OK != err) {
		ERROR("esp_https_ota_begin failed %d\n", err);
		return false;
	}

	return true;
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
	char    url[64];

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
	esp_err_t   err;
	char    url[64];

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













#if 1

/**
 * @file      main.c
 * @brief     Main entry point
 *
 * Copyright joelguittet and mender-mcu-client contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef CONFIG_MENDER_CLIENT_ADD_ON_TROUBLESHOOT
#ifdef CONFIG_MENDER_CLIENT_TROUBLESHOOT_FILE_TRANSFER
#include <esp_littlefs.h>
#endif /* CONFIG_MENDER_CLIENT_TROUBLESHOOT_FILE_TRANSFER */
#endif /* CONFIG_MENDER_CLIENT_ADD_ON_TROUBLESHOOT */

static EventGroupHandle_t mender_client_events;
#define MENDER_CLIENT_EVENT_RESTART (1 << 0)

static mender_err_t network_connect_cb(void)
{
    INFO("Mender client connect network\n");

    /* This callback can be used to configure network connection */
    /* Note that the application can connect the network before if required */
    /* This callback only indicates the mender-client requests network access now */
    /* In this example this helper function configures Wi-Fi or Ethernet, as selected in menuconfig */
    /* Read "Establishing Wi-Fi or Ethernet Connection" section in examples/protocols/README.md for more information */
    if (ESP_OK != example_connect()) {
        ERROR("Unable to connect network\n");
        return MENDER_FAIL;
    }

    return MENDER_OK;
}

/**
 * @brief Network release callback
 * @return MENDER_OK if network is released following the request, error code otherwise
 */
static mender_err_t network_release_cb(void)
{
    INFO("Mender client released network\n");

    /* This callback can be used to release network connection */
    /* Note that the application can keep network activated if required */
    /* This callback only indicates the mender-client doesn't request network access now */
    /* in this example this helper function disconnects the network */
    if (ESP_OK != example_disconnect()) {
        ERROR("Unable to disconnect network\n");
        return MENDER_FAIL;
    }

    return MENDER_OK;
}

static mender_err_t authentication_success_cb(void)
{
    mender_err_t ret;

    INFO("Mender client authenticated\n");

#ifdef CONFIG_MENDER_CLIENT_ADD_ON_TROUBLESHOOT
    /* Activate troubleshoot add-on (deactivated by default) */
    if (MENDER_OK != (ret = mender_troubleshoot_activate())) {
        ESP_LOGE(TAG, "Unable to activate troubleshoot add-on");
        return ret;
    }
#endif /* CONFIG_MENDER_CLIENT_ADD_ON_TROUBLESHOOT */

    /* Validate the image if it is still pending */
    /* Note it is possible to do multiple diagnosic tests before validating the image */
    /* In this example, authentication success with the mender-server is enough */
    if (MENDER_OK != (ret = mender_flash_confirm_image())) {
        ERROR("Unable to validate the image\n");
        return ret;
    }

    return ret;
}

static mender_err_t authentication_failure_cb(void)
{
    static int tries = 0;

    /* Check if confirmation of the image is still pending */
    if (true == mender_flash_is_image_confirmed()) {
        INFO("Mender client authentication failed\n");
        return MENDER_OK;
    }

    /* Increment number of failures */
    tries++;
    ERROR("Mender client authentication failed (%d/%d)\n", tries, CONFIG_EXAMPLE_AUTHENTICATION_FAILS_MAX_TRIES);

    /* Restart the application after several authentication failures with the mender-server */
    /* The image has not been confirmed and the bootloader will now rollback to the previous working image */
    /* Note it is possible to customize this depending of the wanted behavior */
    return (tries >= CONFIG_EXAMPLE_AUTHENTICATION_FAILS_MAX_TRIES) ? MENDER_FAIL : MENDER_OK;
}

/**
 * @brief Deployment status callback
 * @param status Deployment status value
 * @param desc Deployment status description as string
 * @return MENDER_OK if the function succeeds, error code otherwise
 */
static mender_err_t deployment_status_cb(mender_deployment_status_t status, char *desc)
{
    /* We can do something else if required */
    INFO("Deployment status is '%s'\n", desc);

    return MENDER_OK;
}

/**
 * @brief Restart callback
 * @return MENDER_OK if the function succeeds, error code otherwise
 */
static mender_err_t restart_cb(void)
{
    /* Application is responsible to shutdown and restart the system now */
    xEventGroupSetBits(mender_client_events, MENDER_CLIENT_EVENT_RESTART);

    return MENDER_OK;
}

#ifdef CONFIG_MENDER_CLIENT_ADD_ON_CONFIGURE
#ifndef CONFIG_MENDER_CLIENT_CONFIGURE_STORAGE

/**
 * @brief Device configuration updated
 * @param configuration Device configuration
 * @return MENDER_OK if the function succeeds, error code otherwise
 */
static mender_err_t config_updated_cb(mender_keystore_t *configuration)
{

    /* Application can use the new device configuration now */
    /* In this example, we just print the content of the configuration received from the Mender server */
    if (NULL != configuration) {
        size_t index = 0;
        ESP_LOGI(TAG, "Device configuration received from the server");
        while ((NULL != configuration[index].name) && (NULL != configuration[index].value)) {
            ESP_LOGI(TAG, "Key=%s, value=%s", configuration[index].name, configuration[index].value);
            index++;
        }
    }

    return MENDER_OK;
}

#endif /* CONFIG_MENDER_CLIENT_CONFIGURE_STORAGE */
#endif /* CONFIG_MENDER_CLIENT_ADD_ON_CONFIGURE */

#ifdef CONFIG_MENDER_CLIENT_ADD_ON_TROUBLESHOOT
#ifdef CONFIG_MENDER_CLIENT_TROUBLESHOOT_FILE_TRANSFER

static mender_err_t
file_transfer_stat_cb(char *path, size_t **size, uint32_t **uid, uint32_t **gid, uint32_t **mode, time_t **time) {

    assert(NULL != path);
    struct stat  stats;
    mender_err_t ret = MENDER_OK;

    /* Get statistics of file */
    if (0 != stat(path, &stats)) {
        ESP_LOGE(TAG, "Unable to get statistics of file '%s'", path);
        ret = MENDER_FAIL;
        goto FAIL;
    }
    /* Size is optional */
    if (NULL != size) {
        if (NULL == (*size = (size_t *)malloc(sizeof(size_t)))) {
            ESP_LOGE(TAG, "Unable to allocate memory");
            ret = MENDER_FAIL;
            goto FAIL;
        }
        **size = stats.st_size;
    }
    /* UID and GID are optional */
    if (NULL != uid) {
        if (NULL == (*uid = (uint32_t *)malloc(sizeof(uint32_t)))) {
            ESP_LOGE(TAG, "Unable to allocate memory");
            ret = MENDER_FAIL;
            goto FAIL;
        }
        **uid = stats.st_uid;
    }
    if (NULL != gid) {
        if (NULL == (*gid = (uint32_t *)malloc(sizeof(uint32_t)))) {
            ESP_LOGE(TAG, "Unable to allocate memory");
            ret = MENDER_FAIL;
            goto FAIL;
        }
        **gid = stats.st_gid;
    }
    /* Mode is not optional and file must be a regular file to be downloaded by the server */
    if (NULL != mode) {
        if (NULL == (*mode = (uint32_t *)malloc(sizeof(uint32_t)))) {
            ESP_LOGE(TAG, "Unable to allocate memory");
            ret = MENDER_FAIL;
            goto FAIL;
        }
        **mode = stats.st_mode;
    }
    /* Last modification time is optional, format seconds since epoch */
    if (NULL != time) {
        if (NULL == (*time = (time_t *)malloc(sizeof(time_t)))) {
            ESP_LOGE(TAG, "Unable to allocate memory");
            ret = MENDER_FAIL;
            goto FAIL;
        }
        **time = stats.st_mtim.tv_sec;
    }

FAIL:

    return ret;
}

static mender_err_t
file_transfer_open_cb(char *path, char *mode, void **handle) {

    assert(NULL != path);
    assert(NULL != mode);

    /* Open file */
    ESP_LOGI(TAG, "Opening file '%s' with mode '%s'", path, mode);
    if (NULL == (*handle = (void *)fopen(path, mode))) {
        ESP_LOGE(TAG, "Unable to open file '%s'", path);
        return MENDER_FAIL;
    }

    return MENDER_OK;
}

static mender_err_t
file_transfer_read_cb(void *handle, void *data, size_t *length) {

    assert(NULL != handle);
    assert(NULL != data);
    assert(NULL != length);

    /* Read file */
    *length = fread(data, sizeof(uint8_t), *length, (FILE *)handle);

    return MENDER_OK;
}

static mender_err_t
file_transfer_write_cb(void *handle, void *data, size_t length) {

    assert(NULL != handle);
    assert(NULL != data);

    /* Write file */
    if (length != fwrite(data, sizeof(uint8_t), length, (FILE *)handle)) {
        ESP_LOGE(TAG, "Unable to write data to the file");
        return MENDER_FAIL;
    }

    return MENDER_OK;
}

static mender_err_t
file_transfer_close_cb(void *handle) {

    assert(NULL != handle);

    /* Close file */
    ESP_LOGI(TAG, "Closing file");
    fclose((FILE *)handle);

    return MENDER_OK;
}

#endif /* CONFIG_MENDER_CLIENT_TROUBLESHOOT_FILE_TRANSFER */
#ifdef CONFIG_MENDER_CLIENT_TROUBLESHOOT_SHELL

/**
 * @brief Function used to replace a string in the input buffer
 * @param input Input buffer
 * @param search String to be replaced or regex expression
 * @param replace Replacement string
 * @return New string with replacements if the function succeeds, NULL otherwise
 */
static char *
str_replace(char *input, char *search, char *replace) {

    assert(NULL != input);
    assert(NULL != search);
    assert(NULL != replace);

    regex_t    regex;
    regmatch_t match;
    char      *str                   = input;
    char      *output                = NULL;
    size_t     index                 = 0;
    int        previous_match_finish = 0;

    /* Compile expression */
    if (0 != regcomp(&regex, search, REG_EXTENDED)) {
        /* Unable to compile expression */
        ESP_LOGE(TAG, "Unable to compile expression '%s'", search);
        return NULL;
    }

    /* Loop until all search string are replaced */
    bool loop = true;
    while (true == loop) {

        /* Search wanted string */
        if (0 != regexec(&regex, str, 1, &match, 0)) {
            /* No more string to be replaced */
            loop = false;
        } else {
            if (match.rm_so != -1) {

                /* Beginning and ending offset of the match */
                int current_match_start  = (int)(match.rm_so + (str - input));
                int current_match_finish = (int)(match.rm_eo + (str - input));

                /* Reallocate output memory */
                char *tmp = (char *)realloc(output, index + (current_match_start - previous_match_finish) + 1);
                if (NULL == tmp) {
                    ESP_LOGE(TAG, "Unable to allocate memory");
                    regfree(&regex);
                    free(output);
                    return NULL;
                }
                output = tmp;

                /* Copy string from previous match to the beginning of the current match */
                memcpy(&output[index], &input[previous_match_finish], current_match_start - previous_match_finish);
                index += (current_match_start - previous_match_finish);
                output[index] = 0;

                /* Reallocate output memory */
                if (NULL == (tmp = (char *)realloc(output, index + strlen(replace) + 1))) {
                    ESP_LOGE(TAG, "Unable to allocate memory");
                    regfree(&regex);
                    free(output);
                    return NULL;
                }
                output = tmp;

                /* Copy replace string to the output */
                strcat(output, replace);
                index += strlen(replace);

                /* Update previous match ending value */
                previous_match_finish = current_match_finish;
            }
            str += match.rm_eo;
        }
    }

    /* Reallocate output memory */
    char *tmp = (char *)realloc(output, index + (strlen(input) - previous_match_finish) + 1);
    if (NULL == tmp) {
        ESP_LOGE(TAG, "Unable to allocate memory");
        regfree(&regex);
        free(output);
        return NULL;
    }
    output = tmp;

    /* Copy the end of the string after the latest match */
    memcpy(&output[index], &input[previous_match_finish], strlen(input) - previous_match_finish);
    index += (strlen(input) - previous_match_finish);
    output[index] = 0;

    /* Release regex */
    regfree(&regex);

    return output;
}

/**
 * @brief Shell vprintf function used to route logs
 * @param format Log format string
 * @param args Log arguments list
 * @return Length of the log
 */
static int
shell_vprintf(const char *format, va_list args) {

    assert(NULL != format);
    char *buffer, *tmp;
    char  data[256];
    int   length;

    /* Format the log */
    length = vsnprintf(data, sizeof(data), format, args);
    if (length > sizeof(data) - 1) {
        data[sizeof(data) - 1] = '\0';
    }

    /* Ensure new line is "\r\n" to have a proper display of the data in the shell */
    if (NULL == (buffer = strndup(data, length))) {
        goto END;
    }
    if (NULL == (tmp = str_replace(buffer, "\r|\n", "\r\n"))) {
        goto END;
    }
    free(buffer);
    buffer = tmp;

    /* Print log on the shell */
    mender_troubleshoot_shell_print((uint8_t *)buffer, strlen(buffer));

END:

    /* Release memory */
    if (NULL != buffer) {
        free(buffer);
    }

    return length;
}

/**
 * @brief Shell open callback
 * @param terminal_width Terminal width
 * @param terminal_height Terminal height
 * @return MENDER_OK if the function succeeds, error code otherwise
 */
static mender_err_t
shell_open_cb(uint16_t terminal_width, uint16_t terminal_height) {

    /* Shell is connected, print terminal size */
    ESP_LOGI(TAG, "Shell connected with width=%d and height=%d", terminal_width, terminal_height);

    /* Route logs (ESP_LOGx) to the shell */
    esp_log_set_vprintf(shell_vprintf);

    return MENDER_OK;
}

/**
 * @brief Shell resize callback
 * @param terminal_width Terminal width
 * @param terminal_height Terminal height
 * @return MENDER_OK if the function succeeds, error code otherwise
 */
static mender_err_t
shell_resize_cb(uint16_t terminal_width, uint16_t terminal_height) {

    /* Just print terminal size */
    ESP_LOGI(TAG, "Shell resized with width=%d and height=%d", terminal_width, terminal_height);

    return MENDER_OK;
}

/**
 * @brief Shell write data callback
 * @param data Shell data received
 * @param length Length of the data received
 * @return MENDER_OK if the function succeeds, error code otherwise
 */
static mender_err_t
shell_write_cb(void *data, size_t length) {

    mender_err_t ret = MENDER_OK;
    char        *buffer, *tmp;

    /* Ensure new line is "\r\n" to have a proper display of the data in the shell */
    if (NULL == (buffer = (char *)malloc(length + 1))) {
        ESP_LOGE(TAG, "Unable to allocate memory");
        ret = MENDER_FAIL;
        goto END;
    }
    memcpy(buffer, data, length);
    buffer[length] = '\0';
    if (NULL == (tmp = str_replace(buffer, "\r|\n", "\r\n"))) {
        ESP_LOGE(TAG, "Unable to allocate memory");
        ret = MENDER_FAIL;
        goto END;
    }
    free(buffer);
    buffer = tmp;

    /* Send back the data received */
    if (MENDER_OK != (ret = mender_troubleshoot_shell_print((void *)buffer, strlen(buffer)))) {
        ESP_LOGE(TAG, "Unable to print data to the shell");
        ret = MENDER_FAIL;
        goto END;
    }

END:

    /* Release memory */
    if (NULL != buffer) {
        free(buffer);
    }

    return ret;
}

/**
 * @brief Shell close callback
 * @return MENDER_OK if the function succeeds, error code otherwise
 */
static mender_err_t
shell_close_cb(void) {

    /* Route logs back to the UART port */
    esp_log_set_vprintf(vprintf);

    /* Shell has been disconnected */
    ESP_LOGI(TAG, "Shell disconnected");

    return MENDER_OK;
}

#endif /* CONFIG_MENDER_CLIENT_TROUBLESHOOT_SHELL */
#endif /* CONFIG_MENDER_CLIENT_ADD_ON_TROUBLESHOOT */

/**
 * @brief Main function
 */
void mender_main(void)
{

    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if ((ESP_ERR_NVS_NO_FREE_PAGES == ret) || (ESP_ERR_NVS_NEW_VERSION_FOUND == ret)) {
        INFO("Erasing flash...\n");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

#ifdef CONFIG_MENDER_CLIENT_ADD_ON_TROUBLESHOOT
#ifdef CONFIG_MENDER_CLIENT_TROUBLESHOOT_FILE_TRANSFER

    /* Initialize LittleFS */
    esp_vfs_littlefs_conf_t littlefs_conf = {
        .base_path              = "/littlefs",
        .partition_label        = "storage",
        .format_if_mount_failed = true,
        .dont_mount             = false,
    };
    ret = esp_vfs_littlefs_register(&littlefs_conf);
    if (ESP_OK != ret) {
        if (ESP_FAIL == ret) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ESP_ERR_NOT_FOUND == ret) {
            ESP_LOGE(TAG, "Failed to find LittleFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
    }
    ESP_ERROR_CHECK(ret);
    size_t total = 0, used = 0;
    ret = esp_littlefs_info(littlefs_conf.partition_label, &total, &used);
    if (ESP_OK != ret) {
        ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s)", esp_err_to_name(ret));
        esp_littlefs_format(littlefs_conf.partition_label);
    } else {
        ESP_LOGI(TAG, "LittleFS partition size: total: %d, used: %d", total, used);
    }
    ESP_ERROR_CHECK(ret);

#endif /* CONFIG_MENDER_CLIENT_TROUBLESHOOT_FILE_TRANSFER */
#endif /* CONFIG_MENDER_CLIENT_ADD_ON_TROUBLESHOOT */

    /* Initialize network */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Read base MAC address of the device */
    uint8_t mac[6];
    char    mac_address[18];
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));
    sprintf(mac_address, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    INFO("MAC address of the device '%s'\n", mac_address);

    /* Create mender-client event group */
    mender_client_events = xEventGroupCreate();
    ESP_ERROR_CHECK(NULL == mender_client_events);

    /* Retrieve running version of the device */
    esp_app_desc_t         running_app_info;
    const esp_partition_t *running = esp_ota_get_running_partition();
    ESP_ERROR_CHECK(esp_ota_get_partition_description(running, &running_app_info));
    INFO("Running project '%s' version '%s'\n", running_app_info.project_name, running_app_info.version);

    /* Compute artifact name */
    char artifact_name[128];
    sprintf(artifact_name, "%s-v%s", running_app_info.project_name, running_app_info.version);

    /* Retrieve device type */
    char *device_type = running_app_info.project_name;

    /* Initialize mender-client */
    mender_keystore_t         identity[]              = { { .name = "mac", .value = mac_address }, { .name = NULL, .value = NULL } };
    mender_client_config_t    mender_client_config    = { .identity                     = identity,
                                                          .artifact_name                = artifact_name,
                                                          .device_type                  = device_type,
                                                          .host                         = NULL,
                                                          .tenant_token                 = NULL,
                                                          .authentication_poll_interval = 0,
                                                          .update_poll_interval         = 0,
                                                          .recommissioning              = false };
    mender_client_callbacks_t mender_client_callbacks = { .network_connect        = network_connect_cb,
                                                          .network_release        = network_release_cb,
                                                          .authentication_success = authentication_success_cb,
                                                          .authentication_failure = authentication_failure_cb,
                                                          .deployment_status      = deployment_status_cb,
                                                          .restart                = restart_cb };
    ESP_ERROR_CHECK(mender_client_init(&mender_client_config, &mender_client_callbacks));
    INFO("Mender client initialized\n");

    /* Initialize mender add-ons */
#ifdef CONFIG_MENDER_CLIENT_ADD_ON_CONFIGURE
    mender_configure_config_t    mender_configure_config    = { .refresh_interval = 0 };
    mender_configure_callbacks_t mender_configure_callbacks = {
#ifndef CONFIG_MENDER_CLIENT_CONFIGURE_STORAGE
        .config_updated = config_updated_cb,
#endif /* CONFIG_MENDER_CLIENT_CONFIGURE_STORAGE */
    };
    ESP_ERROR_CHECK(mender_client_register_addon(
        (mender_addon_instance_t *)&mender_configure_addon_instance, (void *)&mender_configure_config, (void *)&mender_configure_callbacks));
    INFO("Mender configure add-on registered\n");
#endif /* CONFIG_MENDER_CLIENT_ADD_ON_CONFIGURE */
#ifdef CONFIG_MENDER_CLIENT_ADD_ON_INVENTORY
    mender_inventory_config_t mender_inventory_config = { .refresh_interval = 0 };
    ESP_ERROR_CHECK(mender_client_register_addon((mender_addon_instance_t *)&mender_inventory_addon_instance, (void *)&mender_inventory_config, NULL));
    INFO("Mender inventory add-on registered\n");
#endif /* CONFIG_MENDER_CLIENT_ADD_ON_INVENTORY */
#ifdef CONFIG_MENDER_CLIENT_ADD_ON_TROUBLESHOOT
    mender_troubleshoot_config_t    mender_troubleshoot_config    = { .host = NULL, .healthcheck_interval = 0 };
    mender_troubleshoot_callbacks_t mender_troubleshoot_callbacks = {
#ifdef CONFIG_MENDER_CLIENT_TROUBLESHOOT_FILE_TRANSFER
        .file_transfer = { .stat  = file_transfer_stat_cb,
                           .open  = file_transfer_open_cb,
                           .read  = file_transfer_read_cb,
                           .write = file_transfer_write_cb,
                           .close = file_transfer_close_cb },
#endif /* CONFIG_MENDER_CLIENT_TROUBLESHOOT_FILE_TRANSFER */
#ifdef CONFIG_MENDER_CLIENT_TROUBLESHOOT_PORT_FORWARDING
        .port_forwarding = { .connect = NULL, .send = NULL, .close = NULL },
#endif /* CONFIG_MENDER_CLIENT_TROUBLESHOOT_PORT_FORWARDING */
#ifdef CONFIG_MENDER_CLIENT_TROUBLESHOOT_SHELL
        .shell = { .open = shell_open_cb, .resize = shell_resize_cb, .write = shell_write_cb, .close = shell_close_cb }
#endif /* CONFIG_MENDER_CLIENT_TROUBLESHOOT_SHELL */
    };
    ESP_ERROR_CHECK(mender_client_register_addon(
        (mender_addon_instance_t *)&mender_troubleshoot_addon_instance, (void *)&mender_troubleshoot_config, (void *)&mender_troubleshoot_callbacks));
    ESP_LOGI(TAG, "Mender troubleshoot add-on registered");
#endif /* CONFIG_MENDER_CLIENT_ADD_ON_TROUBLESHOOT */

#ifdef CONFIG_MENDER_CLIENT_ADD_ON_CONFIGURE
    /* Get mender configuration (this is just an example to illustrate the API) */
    mender_keystore_t *configuration;
    if (MENDER_OK != mender_configure_get(&configuration)) {
        ERROR("Unable to get mender configuration\n");
    } else if (NULL != configuration) {
        size_t index = 0;
        INFO("Device configuration retrieved\n");
        while ((NULL != configuration[index].name) && (NULL != configuration[index].value)) {
            INFO("Key=%s, value=%s\n", configuration[index].name, configuration[index].value);
            index++;
        }
        mender_utils_keystore_delete(configuration);
    }
#endif /* CONFIG_MENDER_CLIENT_ADD_ON_CONFIGURE */

#ifdef CONFIG_MENDER_CLIENT_ADD_ON_INVENTORY
    /* Set mender inventory (this is just an example to illustrate the API) */
    mender_keystore_t inventory[] = { { .name = "esp-idf", .value = IDF_VER },
                                      { .name = "mender-mcu-client", .value = mender_client_version() },
                                      { .name = "latitude", .value = "45.8325" },
                                      { .name = "longitude", .value = "6.864722" },
                                      { .name = NULL, .value = NULL } };
    if (MENDER_OK != mender_inventory_set(inventory)) {
        ERROR("Unable to set mender inventory\n");
    }
#endif /* CONFIG_MENDER_CLIENT_ADD_ON_INVENTORY */

    /* Finally activate mender client */
    if (MENDER_OK != mender_client_activate()) {
        ERROR("Unable to activate mender-client\n");
        goto RELEASE;
    }

    /* Wait for mender-mcu-client events */
    xEventGroupWaitBits(mender_client_events, MENDER_CLIENT_EVENT_RESTART, pdTRUE, pdFALSE, portMAX_DELAY);

RELEASE:

    /* Deactivate and release mender-client */
    mender_client_deactivate();
    mender_client_exit();

    /* Release event group */
    vEventGroupDelete(mender_client_events);

    /* Restart */
    INFO("Restarting system\n");
    esp_restart();
}

#endif
