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
#include "main.h"
#include "cli.h"
#include "cmd.h"
#include "wifi.h"
#include "pump.h"
#include "ctrl.h"
#include "adc.h"
#include "led.h"
#include "nvs.h"
#include "config.h"
#include "max30001.h"
#include "max17049.h"
#include "mender_ota.h"

#define BUF_SIZE    1024

void ESP_printErr(int err)
{
	char* pStr = NULL;
	if (err == ESP_OK) {
		return;
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

		case ESP_ERR_NVS_INVALID_HANDLE:
			pStr = "ESP_ERR_NVS_INVALID_HANDLE";
			break;

		case ESP_ERR_NVS_READ_ONLY:
			pStr = "ESP_ERR_NVS_READ_ONLY";
			break;
	}

	if (pStr) {
		ERROR("failed %s\n", pStr);
	} else {
		ERROR("failed 0x%x\n", err);
	}
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

void app_main(void)
{
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	uart_init();
	CLI_init();
	CFG_init();
	NVS_init();
	MENDER_init();
	CMD_init(NULL);
	ADC_init();

	max30001_init();
	fg_init();
	LED_init();
	WIFI_init();

	PMP_init();
	CTRL_init();

	esp_log_level_set("*", ESP_LOG_ERROR);

	PRINT("\n");
	PRINT(" ==================================================\n");
	PRINT("  Ready.\n");
	PRINT(" =================================================\n\n");
}
