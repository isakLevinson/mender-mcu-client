/* Wi-Fi iperf Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/


#define DEF_DBG_MODULE	DBG_MODULE_MAIN

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include <errno.h>
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"

#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_heap_trace.h"
#include "main.h"
#include "cli.h"
#include "cmd.h"
#include "wifi.h"
#include "tls.h"
#include "ntp.h"
#include "pump.h"
#include "ctrl.h"
#include "adc.h"
#include "mmi.h"
#include "nvs.h"
#include "factory.h"
#include "config.h"
#include "max30001.h"
#include "max17049.h"
#include "mender_ota.h"
#include "ota.h"
#include "vault.h"

#define BUF_SIZE    1024
#define NUM_RECORDS 50
static heap_trace_record_t trace_record[NUM_RECORDS];

char* ESP_getErrStr(int err)
{
	char* pStr = NULL;
	static char str[16];

	if (err == ESP_OK) {
		return "";
	}

	switch (err) {
		case ESP_ERR_NVS_NOT_FOUND:
			pStr = "ESP_ERR_NVS_NOT_FOUND";
			break;
		case ESP_ERR_NVS_NOT_INITIALIZED:
			pStr = "ESP_ERR_NVS_NOT_INITIALIZED";
			break;
		case ESP_ERR_NO_MEM:
			pStr = "ESP_ERR_NO_MEM";
			break;
		case ESP_ERR_INVALID_ARG:
			pStr = "ESP_ERR_INVALID_ARG";
			break;
		case ESP_ERR_NVS_INVALID_LENGTH:
			pStr = "ESP_ERR_NVS_INVALID_LENGTH";
			break;

		case ESP_ERR_TIMEOUT:
			pStr = "ESP_ERR_TIMEOUT";
			break;

		case ESP_ERR_INVALID_STATE:
			pStr = "ESP_ERR_INVALID_STATE";
			break;

		case ESP_ERR_NOT_SUPPORTED:
			pStr = "ESP_ERR_NOT_SUPPORTED";
			break;

		case ESP_ERR_NOT_FOUND:
			pStr = "ESP_ERR_NOT_FOUND";
			break;

		case ESP_ERR_NVS_INVALID_HANDLE:
			pStr = "ESP_ERR_NVS_INVALID_HANDLE";
			break;
		case ESP_ERR_NVS_READ_ONLY:
			pStr = "ESP_ERR_NVS_READ_ONLY";
			break;
		case ESP_ERR_NVS_NOT_ENOUGH_SPACE:
			pStr = "ESP_ERR_NVS_NOT_ENOUGH_SPACE";
			break;
		case ESP_ERR_WIFI_MODE:
			pStr = "ESP_ERR_WIFI_MODE";
			break;
	}

	if (pStr) {
		return pStr;
	}

	sprintf(str, "%02x", err);
	return str;
}

void uart_init(void)
{
	uart_config_t uart_config = {
		.baud_rate = ECHO_UART_BAUD_RATE,
		.data_bits = UART_DATA_8_BITS,
		.parity    = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_DEFAULT,
	};
	int intr_alloc_flags = 0;

#if CONFIG_UART_ISR_IN_IRAM
	intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

	ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
	ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));
	ESP_ERROR_CHECK(uart_set_pin(ECHO_UART_PORT_NUM, ECHO_TEST_TXD, ECHO_TEST_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

	ESP_ERROR_CHECK(uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
	ESP_ERROR_CHECK(uart_param_config(CONFIG_ESP_CONSOLE_UART_NUM, &uart_config));
	ESP_ERROR_CHECK(uart_set_pin(CONFIG_ESP_CONSOLE_UART_NUM, -1, -1, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

bool app_trace_start(void)
{
	int err;
	err = heap_trace_init_standalone(trace_record, NUM_RECORDS);
	if (ESP_OK != err) {
		ERROR("heap_trace_init_standalone %x\n", err);
		return false;
	}

	err = heap_trace_start(HEAP_TRACE_ALL);
	if (ESP_OK != err) {
		ERROR("heap_trace_start %x\n", err);
		return false;
	}

	return true;
}

void app_trace_stop(void)
{
	int err;
	err = heap_trace_stop();
}

void app_main(void)
{
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	app_trace_start();

	uart_init();
	CLI_init();
	FACTORY_init();
	CFG_init();
	OTA_init();
	NVS_init();
	MENDER_init();
	CMD_init(NULL);
	ADC_init();

	max30001_init();
	fg_init();
	MMI_init();
	WIFI_init();
	ntp_init();
	TLS_init();
	VAULT_init();

	PMP_init();
	CTRL_init();

	esp_log_level_set("*", ESP_LOG_INFO);

	PRINT("\n");
	PRINT(" ==================================================\n");
	PRINT("  Ready.\n");
	PRINT(" =================================================\n\n");

	app_trace_stop();
}
