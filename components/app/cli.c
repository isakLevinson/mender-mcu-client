

#define DEF_DBG_MODULE	DBG_MODULE_MAIN

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "cmd_decl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "iperf.h"
#include "esp_coexist.h"

#include <sys/socket.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "iperf.h"

#include "driver/uart.h"
#include "main.h"

#include "cli.h"
#include "fifo.h"

static struct {
	//osThreadId			taskHandle;
	DBG_DECODE_INST		decoder;
	SemaphoreHandle_t	mutex;
} g_cliDb;


static uint32_t _mutexGet(uint8_t devMask)
{
	xSemaphoreTake(g_cliDb.mutex, portMAX_DELAY);

	return true;
}

static bool _mutexPost(uint8_t devMask, uint32_t state)
{
	xSemaphoreGive(g_cliDb.mutex);

	return true;
}

static bool _puts(uint8_t devBitmap, char* i_pStr, uint16_t size)
{
	bool	retVal;
	char	decodedBuf[256];
	uint16_t	decodedSize;
	
	if (devBitmap & DBG_OUT_STREAM_DEVICE_MASK_CLI) {
		retVal = DBG_PRINT_decode(&g_cliDb.decoder, (uint8_t*)i_pStr, size, false, true, decodedBuf, &decodedSize, NULL);
		if (retVal) {
            uart_write_bytes(CONFIG_ESP_CONSOLE_UART_NUM, decodedBuf, decodedSize);
		}
	}

	return true;
}

static void* _alloc(uint32_t size)
{
	void*	pBuf;
	pBuf = malloc(size);
	return pBuf;
}

static void _free(void* i_pBuf)
{
	free(i_pBuf);
}


static void _task(void *arg)
{
    char c;
//    char str[256];
    size_t length;

    PRINT("Ready.\n");

//    strcpy(str, "\r\nCLI task Ready\r\n");
//    uart_write_bytes(ECHO_UART_PORT_NUM, str, strlen(str));
//    uart_write_bytes(CONFIG_ESP_CONSOLE_UART_NUM, str, strlen(str));

    while(true) {
        uart_get_buffered_data_len(CONFIG_ESP_CONSOLE_UART_NUM, &length);

        if (length > 1) {
            length = 1;
        }

        if (!length) {
            vTaskDelay(10);
        } else {
            length = uart_read_bytes(CONFIG_ESP_CONSOLE_UART_NUM, &c, length, 1);
            if (length) {
                //uart_write_bytes(CONFIG_ESP_CONSOLE_UART_NUM, &c, 1);
                DBG_MENU_handler(c);
            }
        }
    }

    vTaskDelete(NULL);





}

static bool dbgVer(uint8_t argc, char** argv)
{
    PRINT("ver\n");
    return true;
}


DEBUG_MENU_START(g_menu)
	DEBUG_MENU_CMD("ver",			NULL,		NULL, dbgVer)
DEBUG_MENU_END

bool	CLI_init(void)
{
	DBG_PRINT_CONFIG	dbgPrintCfg = {
		.cbPuts				= _puts,
		.cbMutexGet			=_mutexGet,
		.cbMutexRelease		= _mutexPost,
		.cbGetTime64		= NULL,
	};

	DBG_MENU_CONFIG cfg = {
		.eol		= '\r',
		.ignore		= '\n',
		.pPrompt	= "\\w\\$ ",
		.pfAlloc	= _alloc,
		.pfFree		= _free,
	};

	g_cliDb.mutex = xSemaphoreCreateMutex();

	DBG_PRINT_decodeInit(&g_cliDb.decoder);

	DBG_PRINT_init(&dbgPrintCfg);

	DBG_MENU_init(&cfg);

	DBG_PRINT_addMenu();
	DBG_TREE_add("/etc",		g_menu);
	
	FIFO_addMenu();

    int ret;
    char str[256];

    strcpy(str, "\r\nCLI Ready\r\n");
    uart_write_bytes(ECHO_UART_PORT_NUM, str, strlen(str));
    uart_write_bytes(CONFIG_ESP_CONSOLE_UART_NUM, str, strlen(str));


    ret = xTaskCreate(_task, IPERF_TRAFFIC_TASK_NAME, IPERF_TRAFFIC_TASK_STACK, NULL, IPERF_TRAFFIC_TASK_PRIORITY, NULL);
    if (ret != pdPASS) {
        //ERROR
        return false;
    }
    return true;
}