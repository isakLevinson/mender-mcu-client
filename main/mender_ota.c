
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

#include "main.h"
#include "config.h"
#include "cmd.h"
#include "factory.h"

#ifdef CONFIG_MENDER_CLIENT_ADD_ON_TROUBLESHOOT
#ifdef CONFIG_MENDER_CLIENT_TROUBLESHOOT_FILE_TRANSFER
#include <esp_littlefs.h>
#endif /* CONFIG_MENDER_CLIENT_TROUBLESHOOT_FILE_TRANSFER */
#endif /* CONFIG_MENDER_CLIENT_ADD_ON_TROUBLESHOOT */

static struct {
	esp_app_desc_t	running_app_info;
	bool			active;
	char			url[64];
	char			token[64];
} g_mender;

static void _restart(void)
{
	esp_restart();
}

static mender_err_t network_connect_cb(void)
{
	INFO("Mender client connect network\n");

	/* This callback can be used to configure network connection */
	/* Note that the application can connect the network before if required */
	/* This callback only indicates the mender-client requests network access now */
	/* In this example this helper function configures Wi-Fi or Ethernet, as selected in menuconfig */
	/* Read "Establishing Wi-Fi or Ethernet Connection" section in examples/protocols/README.md for more information */
#if 0
	if (ESP_OK != example_connect()) {
		ERROR("Unable to connect network\n");
		return MENDER_FAIL;
	}
#endif
	return MENDER_OK;
}

static mender_err_t network_release_cb(void)
{
	INFO("Mender client released network\n");

	/* This callback can be used to release network connection */
	/* Note that the application can keep network activated if required */
	/* This callback only indicates the mender-client doesn't request network access now */
	/* in this example this helper function disconnects the network */

#if 0
	if (ESP_OK != example_disconnect()) {
		ERROR("Unable to disconnect network\n");
		return MENDER_FAIL;
	}
#endif
	return MENDER_OK;
}

static mender_err_t authentication_success_cb(void)
{
	mender_err_t ret;

	INFO("Mender client authenticated !\n");

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
		//return ret;
	}

	return MENDER_OK;
}

static mender_err_t authentication_failure_cb(void)
{
	static int tries = 0;

	/* Check if confirmation of the image is still pending */
	if (true == mender_flash_is_image_confirmed()) {
		INFO("Mender client authentication ok!\n");
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
static mender_err_t deployment_status_cb(mender_deployment_status_t status, char* desc)
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
	INFO("restart_cb\n");
	/* Application is responsible to shutdown and restart the system now */

	CFG_set(cfg_id_ota_updated,  "1");
	//	CMD_sendVersionEvent();

	// just give anogh time for the event to be sent
	vTaskDelay(500);

	_restart();

	return MENDER_OK;
}

mender_err_t update_http_config_cb(esp_http_client_config_t* cfg)
{
	bool	ret;

	INFO("update_http_config_cb\n");

	cfg->cert_pem = malloc(4096);
	if (!cfg->cert_pem) {
		ERROR("failed allocating cert_pem\n");
		goto err;
	}

	ret = CFG_get(cfg_id_ca_pem, cfg->cert_pem, 4096);
	if (!ret) {
		ERROR("CFG_get failed cert_pem\n");
		goto err;
	}

	cfg->skip_cert_common_name_check = true;
	cfg->crt_bundle_attach			 = NULL;
	INFO("added CA cert to config\n");

	return MENDER_OK;

err:
	ERROR("update_http_config_cb failed\n");
	if (cfg->cert_pem) {
		free(cfg->cert_pem);
		cfg->cert_pem = NULL;
	}

	return MENDER_FAIL;
}

#ifdef CONFIG_MENDER_CLIENT_ADD_ON_CONFIGURE
#ifndef CONFIG_MENDER_CLIENT_CONFIGURE_STORAGE

/**
 * @brief Device configuration updated
 * @param configuration Device configuration
 * @return MENDER_OK if the function succeeds, error code otherwise
 */
static mender_err_t config_updated_cb(mender_keystore_t* configuration)
{
	INFO("config_updated_cb\n");
	/* Application can use the new device configuration now */
	/* In this example, we just print the content of the configuration received from the Mender server */
	if (NULL != configuration) {
		size_t index = 0;
		INFO("Device configuration received from the server\n");
		while ((NULL != configuration[index].name) && (NULL != configuration[index].value)) {
			INFO("Key=%s, value=%s\n", configuration[index].name, configuration[index].value);
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
file_transfer_stat_cb(char* path, size_t** size, uint32_t** uid, uint32_t** gid, uint32_t** mode, time_t** time)
{

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
		if (NULL == (*size = (size_t*)malloc(sizeof(size_t)))) {
			ESP_LOGE(TAG, "Unable to allocate memory");
			ret = MENDER_FAIL;
			goto FAIL;
		}
		** size = stats.st_size;
	}
	/* UID and GID are optional */
	if (NULL != uid) {
		if (NULL == (*uid = (uint32_t*)malloc(sizeof(uint32_t)))) {
			ESP_LOGE(TAG, "Unable to allocate memory");
			ret = MENDER_FAIL;
			goto FAIL;
		}
		** uid = stats.st_uid;
	}
	if (NULL != gid) {
		if (NULL == (*gid = (uint32_t*)malloc(sizeof(uint32_t)))) {
			ESP_LOGE(TAG, "Unable to allocate memory");
			ret = MENDER_FAIL;
			goto FAIL;
		}
		** gid = stats.st_gid;
	}
	/* Mode is not optional and file must be a regular file to be downloaded by the server */
	if (NULL != mode) {
		if (NULL == (*mode = (uint32_t*)malloc(sizeof(uint32_t)))) {
			ESP_LOGE(TAG, "Unable to allocate memory");
			ret = MENDER_FAIL;
			goto FAIL;
		}
		** mode = stats.st_mode;
	}
	/* Last modification time is optional, format seconds since epoch */
	if (NULL != time) {
		if (NULL == (*time = (time_t*)malloc(sizeof(time_t)))) {
			ESP_LOGE(TAG, "Unable to allocate memory");
			ret = MENDER_FAIL;
			goto FAIL;
		}
		** time = stats.st_mtim.tv_sec;
	}

FAIL:

	return ret;
}

static mender_err_t
file_transfer_open_cb(char* path, char* mode, void** handle)
{

	assert(NULL != path);
	assert(NULL != mode);

	/* Open file */
	ESP_LOGI(TAG, "Opening file '%s' with mode '%s'", path, mode);
	if (NULL == (*handle = (void*)fopen(path, mode))) {
		ESP_LOGE(TAG, "Unable to open file '%s'", path);
		return MENDER_FAIL;
	}

	return MENDER_OK;
}

static mender_err_t
file_transfer_read_cb(void* handle, void* data, size_t* length)
{

	assert(NULL != handle);
	assert(NULL != data);
	assert(NULL != length);

	/* Read file */
	*length = fread(data, sizeof(uint8_t), *length, (FILE*)handle);

	return MENDER_OK;
}

static mender_err_t
file_transfer_write_cb(void* handle, void* data, size_t length)
{

	assert(NULL != handle);
	assert(NULL != data);

	/* Write file */
	if (length != fwrite(data, sizeof(uint8_t), length, (FILE*)handle)) {
		ESP_LOGE(TAG, "Unable to write data to the file");
		return MENDER_FAIL;
	}

	return MENDER_OK;
}

static mender_err_t
file_transfer_close_cb(void* handle)
{

	assert(NULL != handle);

	/* Close file */
	ESP_LOGI(TAG, "Closing file");
	fclose((FILE*)handle);

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
static char*
str_replace(char* input, char* search, char* replace)
{

	assert(NULL != input);
	assert(NULL != search);
	assert(NULL != replace);

	regex_t    regex;
	regmatch_t match;
	char*      str                   = input;
	char*      output                = NULL;
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
				char* tmp = (char*)realloc(output, index + (current_match_start - previous_match_finish) + 1);
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
				if (NULL == (tmp = (char*)realloc(output, index + strlen(replace) + 1))) {
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
	char* tmp = (char*)realloc(output, index + (strlen(input) - previous_match_finish) + 1);
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
shell_vprintf(const char* format, va_list args)
{

	assert(NULL != format);
	char* buffer, *tmp;
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
	mender_troubleshoot_shell_print((uint8_t*)buffer, strlen(buffer));

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
shell_open_cb(uint16_t terminal_width, uint16_t terminal_height)
{

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
shell_resize_cb(uint16_t terminal_width, uint16_t terminal_height)
{

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
shell_write_cb(void* data, size_t length)
{

	mender_err_t ret = MENDER_OK;
	char*        buffer, *tmp;

	/* Ensure new line is "\r\n" to have a proper display of the data in the shell */
	if (NULL == (buffer = (char*)malloc(length + 1))) {
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
	if (MENDER_OK != (ret = mender_troubleshoot_shell_print((void*)buffer, strlen(buffer)))) {
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
shell_close_cb(void)
{

	/* Route logs back to the UART port */
	esp_log_set_vprintf(vprintf);

	/* Shell has been disconnected */
	ESP_LOGI(TAG, "Shell disconnected");

	return MENDER_OK;
}

#endif /* CONFIG_MENDER_CLIENT_TROUBLESHOOT_SHELL */
#endif /* CONFIG_MENDER_CLIENT_ADD_ON_TROUBLESHOOT */

bool MENDER_version(char** ppProjName, char** ppVer)
{
	const esp_partition_t* partition = esp_ota_get_running_partition();
	ESP_ERROR_CHECK(esp_ota_get_partition_description(partition, &g_mender.running_app_info));

	if (ppProjName) {
		*ppProjName = g_mender.running_app_info.project_name;
	}

	if (ppVer) {
		*ppVer = g_mender.running_app_info.version;
	}

	return true;
}


static bool _init(void)
{
	bool	ret;
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
	char*	project_name;
	char*	version;
	char	sn[64];

	MENDER_version(&project_name, &version);
	ret = CFG_get(cfg_id_sn, sn, sizeof(sn));
	if (!ret) {
		ERROR("SN not set. aborting init\n");
		return false;
	}

	/* Retrieve running version of the device */
	INFO("Running project '%s' version '%s'\n", project_name, version);

	/* Compute artifact name */
	static char artifact_name[128];
	sprintf(artifact_name, "%s-v%s", project_name, version);

	INFO("artifact_name: %s\n", artifact_name);

	/* Retrieve device type */
	char* device_type = project_name;

	/* Initialize mender-client */
	mender_keystore_t  identity[]              = { { .name = "sn", .value = sn }, { .name = NULL, .value = NULL } };
	mender_client_config_t    mender_client_config    = {
		.identity                     = identity,
		.artifact_name                = artifact_name,
		.device_type                  = device_type,
		.tenant_token                 = NULL,
		.authentication_poll_interval = -1,
		.update_poll_interval         = -1,	// only attempt once
		.recommissioning              = false
	};

	mender_client_callbacks_t mender_client_callbacks = {
		.network_connect        = network_connect_cb,
		.network_release        = network_release_cb,
		.authentication_success = authentication_success_cb,
		.authentication_failure = authentication_failure_cb,
		.deployment_status      = deployment_status_cb,
		.restart                = restart_cb,
		.update_http_config_cb	= update_http_config_cb,
	};

	CFG_get(cfg_id_ota_url, g_mender.url, sizeof(g_mender.url));
	CFG_get(cfg_id_ota_token, g_mender.token, sizeof(g_mender.token));
	mender_client_config.host 			= g_mender.url;
	mender_client_config.tenant_token	= g_mender.token;

	ESP_ERROR_CHECK(mender_client_init(&mender_client_config, &mender_client_callbacks));
	INFO("Mender client initialized\n");

	/* Initialize mender add-ons */
#ifdef CONFIG_MENDER_CLIENT_ADD_ON_CONFIGURE
	mender_configure_config_t    mender_configure_config    = { .refresh_interval = -1 };
	mender_configure_callbacks_t mender_configure_callbacks = {
#ifndef CONFIG_MENDER_CLIENT_CONFIGURE_STORAGE
		.config_updated = config_updated_cb,
#endif /* CONFIG_MENDER_CLIENT_CONFIGURE_STORAGE */
	};

	ESP_ERROR_CHECK(mender_client_register_addon(
	        (mender_addon_instance_t*)&mender_configure_addon_instance, (void*)&mender_configure_config, (void*)&mender_configure_callbacks));
	INFO("Mender configure add-on registered\n");
#endif /* CONFIG_MENDER_CLIENT_ADD_ON_CONFIGURE */
#ifdef CONFIG_MENDER_CLIENT_ADD_ON_INVENTORY
	mender_inventory_config_t mender_inventory_config = { .refresh_interval = 0 };
	ESP_ERROR_CHECK(mender_client_register_addon((mender_addon_instance_t*)&mender_inventory_addon_instance, (void*)&mender_inventory_config, NULL));
	INFO("Mender inventory add-on registered\n");
#endif /* CONFIG_MENDER_CLIENT_ADD_ON_INVENTORY */
#ifdef CONFIG_MENDER_CLIENT_ADD_ON_TROUBLESHOOT
	mender_troubleshoot_config_t    mender_troubleshoot_config    = { .host = NULL, .healthcheck_interval = 0 };
	mender_troubleshoot_callbacks_t mender_troubleshoot_callbacks = {
#ifdef CONFIG_MENDER_CLIENT_TROUBLESHOOT_FILE_TRANSFER
		.file_transfer = {
			.stat  = file_transfer_stat_cb,
			.open  = file_transfer_open_cb,
			.read  = file_transfer_read_cb,
			.write = file_transfer_write_cb,
			.close = file_transfer_close_cb
		},
#endif /* CONFIG_MENDER_CLIENT_TROUBLESHOOT_FILE_TRANSFER */
#ifdef CONFIG_MENDER_CLIENT_TROUBLESHOOT_PORT_FORWARDING
		.port_forwarding = { .connect = NULL, .send = NULL, .close = NULL },
#endif /* CONFIG_MENDER_CLIENT_TROUBLESHOOT_PORT_FORWARDING */
#ifdef CONFIG_MENDER_CLIENT_TROUBLESHOOT_SHELL
		.shell = { .open = shell_open_cb, .resize = shell_resize_cb, .write = shell_write_cb, .close = shell_close_cb }
#endif /* CONFIG_MENDER_CLIENT_TROUBLESHOOT_SHELL */
	};
	ESP_ERROR_CHECK(mender_client_register_addon(
	        (mender_addon_instance_t*)&mender_troubleshoot_addon_instance, (void*)&mender_troubleshoot_config, (void*)&mender_troubleshoot_callbacks));
	ESP_LOGI(TAG, "Mender troubleshoot add-on registered");
#endif /* CONFIG_MENDER_CLIENT_ADD_ON_TROUBLESHOOT */

#ifdef CONFIG_MENDER_CLIENT_ADD_ON_CONFIGURE
	/* Get mender configuration (this is just an example to illustrate the API) */
	mender_keystore_t* configuration;
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
	} else {
		ERROR("configuration: %x\n", configuration);
	}
#endif /* CONFIG_MENDER_CLIENT_ADD_ON_CONFIGURE */

#ifdef CONFIG_MENDER_CLIENT_ADD_ON_INVENTORY
	/* Set mender inventory (this is just an example to illustrate the API) */
	mender_keystore_t inventory[] = { { .name = "esp-idf", .value = IDF_VER },
		{ .name = "mender-mcu-client", .value = mender_client_version() },
		{ .name = "latitude", .value = "45.8325" },
		{ .name = "longitude", .value = "6.864722" },
		{ .name = NULL, .value = NULL }
	};
	if (MENDER_OK != mender_inventory_set(inventory)) {
		ERROR("Unable to set mender inventory\n");
	} else {
		INFO("mender_inventory_set ok\n");
	}
#endif /* CONFIG_MENDER_CLIENT_ADD_ON_INVENTORY */
	return true;
}

static void _stop(void)
{
	if (!g_mender.active) {
		WARN("mender mot active\n");
		return;
	}
	/* Deactivate and release mender-client */
	mender_client_deactivate();
	//mender_client_exit();
	g_mender.active = false;
}

static void _start(void)
{
	if (g_mender.active) {
		WARN("mender client already running\n");
		return;
	}

	/* Finally activate mender client */
	if (MENDER_OK != mender_client_activate()) {
		ERROR("Unable to activate mender-client\n");
		goto stop;
	}

	g_mender.active = true;
	INFO("mender_client_activate ok\n");

	return;

stop:
	INFO("RELEASE:\n");
	_stop();
}

void MENDER_execute(void)
{
	if (!g_mender.active) {
		WARN("mender mot active\n");
		return;
	}
	/* Deactivate and release mender-client */
	mender_client_execute();
}

static bool dbgInit(uint8_t argc, char** argv)
{
	_init();
	return true;
}

static bool dbgStart(uint8_t argc, char** argv)
{
	_start();
	return true;
}

static bool dbgStop(uint8_t argc, char** argv)
{
	_stop();
	return true;
}

static bool dbgExecute(uint8_t argc, char** argv)
{
	MENDER_execute();
	return true;
}

static bool dbgRestart(uint8_t argc, char** argv)
{
	_restart();
	return true;
}

static bool dbgTest(uint8_t argc, char** argv)
{
	bool    ret;
	int     err;
	bool    isConfirmed = false;
	bool    confirm     = false;
	bool    markValid   = false;

// *INDENT-OFF*
	ARGS_ENTRY_BEGIN(args)
		ARGS_ENTRY("ic",	ARGS_TYPE_SWITCH,	0,	"mender is confirmed",	    &isConfirmed)
		ARGS_ENTRY("mc",	ARGS_TYPE_SWITCH,	0,	"mender confirm",   	    &confirm)
		ARGS_ENTRY("v",		ARGS_TYPE_SWITCH,	0,	"mark image as valid",	    &markValid)
	ARGS_ENTRY_END()
// *INDENT-ON*

	ret = ARGS_readValues(argc, argv, args, NULL, NULL);
	if (!ret) {
		return false;
	}

	if (isConfirmed) {
		ret = mender_flash_is_image_confirmed();
		PRINT("%d\n", ret);
	}

	if (confirm) {
		mender_err_t mender_err;
		mender_err = mender_flash_confirm_image();
		PRINT("%d\n", mender_err);
	}

	if (markValid) {
		err = esp_ota_mark_app_valid_cancel_rollback();
		PRINT("%d\n", err);
	}

	return true;
}

static bool dbgStatus(uint8_t argc, char** argv)
{
	esp_err_t err;
	mender_keystore_t* configuration;
	mender_err_t    mender_err;

	esp_ota_img_states_t   img_state;
	const esp_partition_t* partition = esp_ota_get_running_partition();
	err = esp_ota_get_state_partition(partition, &img_state);
	if (ESP_OK != err) {
		ERROR("esp_ota_get_state_partition %s\n", ESP_getErrStr(err));
	}

	PRINT("label: %s\n", partition->label);
	PRINT("type : %d / %d\n", partition->type, partition->subtype);
	PRINT("addr : 0x%x size: 0x%x\n", partition->address, partition->size);
	PRINT("enc: %d ro:%d\n", partition->encrypted, partition->readonly);

	mender_err = mender_configure_get(&configuration);
	if (MENDER_OK != mender_err) {
		ERROR("Unable to get mender configuration\n");
	} else if (configuration) {
		size_t index = 0;
		PRINT("Device configuration retrieved\n");
		while ((NULL != configuration[index].name) && (NULL != configuration[index].value)) {
			PRINT("Key=%s, value=%s\n", configuration[index].name, configuration[index].value);
			index++;
		}
		mender_utils_keystore_delete(configuration);
	} else {
		ERROR("configuration: %x\n", configuration);
	}

	return true;
}

// *INDENT-OFF*
DEBUG_MENU_START(g_menu)
	DEBUG_MENU_DIR("mender_ota",	NULL)
		DEBUG_MENU_CMD("status",	NULL,		    NULL, dbgStatus)
		DEBUG_MENU_CMD("init",		NULL,		    NULL, dbgInit)
		DEBUG_MENU_CMD("start",		NULL,		    NULL, dbgStart)
		DEBUG_MENU_CMD("stop",		NULL,		    NULL, dbgStop)
		DEBUG_MENU_CMD("execute",	NULL,		    NULL, dbgExecute)
		DEBUG_MENU_CMD("test",  	NULL,		    NULL, dbgTest)
		DEBUG_MENU_CMD("restart",	NULL,		    NULL, dbgRestart)
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*

void MENDER_init(void)
{
	bool	ret;
	DBG_TREE_add("/",		g_menu);

	ret = _init();
	if (!ret) {
		return;
	}

	_start();
}
