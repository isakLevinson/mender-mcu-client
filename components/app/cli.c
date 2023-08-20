

#define DEF_DBG_MODULE	DBG_MODULE_MAIN

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "iperf.h"

#include "driver/uart.h"
#include "esp_timer.h"

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

int64_t _getTime(void)
{
	return esp_timer_get_time();
}

static void _task(void *arg)
{
    char c;
    size_t length;

    PRINT("Ready.\n");

    while(true) {
        length = uart_read_bytes(CONFIG_ESP_CONSOLE_UART_NUM, &c, 1, 1);
        if (length) {
            DBG_MENU_handler(c);
        }
    }

    vTaskDelete(NULL);
}

bool CLI_getc(char* o_pChar)
{
    size_t length;

    length = uart_read_bytes(CONFIG_ESP_CONSOLE_UART_NUM, o_pChar, 1, 1);

	return (length > 0);
}


static bool dbgVer(uint8_t argc, char** argv)
{
    PRINT("ver %d %d %d %d %d \n",
		SOFTWARE_MAJOR_VERSION,
		SOFTWARE_MINOR_VERSION,
		SOFTWARE_PATCH_VERSION,
		HARDWARE_MAJOR_VERSION,
		HARDWARE_MINOR_VERSION);

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
		.cbGetTime64		= _getTime,
	};

	DBG_MENU_CONFIG cfg = {
		.eol		= '\r',
		.ignore		= '\n',
		.pPrompt	= PROMPT " \\w\\$ ",
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