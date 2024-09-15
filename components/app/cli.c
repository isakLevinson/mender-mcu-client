

#define DEF_DBG_MODULE	DBG_MODULE_MAIN

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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
		retVal = DBG_PRINT_decode(&g_cliDb.decoder, (uint8_t*)i_pStr, size, decodedBuf, &decodedSize, NULL);
		if (retVal) {
            uart_write_bytes(CONFIG_ESP_CONSOLE_UART_NUM, decodedBuf, decodedSize);
		}
	}

	return true;
}

static void* _alloc(void* ptr, uint32_t size)
{
	void*	pBuf;
	pBuf = malloc(size);
	return pBuf;
}

static void _free(void* i_pBuf)
{
	free(i_pBuf);
}

uint64_t _getTime(void)
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
    PRINT("sw:%d.%d.%d\n",
		SW_VERSION_MAJOR,
		SW_VERSION_MINOR,
		SW_VERSION_BUILD);

    PRINT("hw:%d.%d.%d\n",
		HW_VERSION_MAJOR,
		HW_VERSION_MINOR,
		HW_VERSION_BUILD);

	//PRINT_BUF("hash",	PRINT_BUF_STYLE_HEX_NL, i_pBuf, size);

    return true;
}

static bool dbgPs(uint8_t argc, char** argv)
{
	UBaseType_t	uxArraySize;
	TaskStatus_t pxTaskStatusArray[32];
	uint8_t		i;
	unsigned long pulTotalRunTime;

	uxArraySize = uxTaskGetNumberOfTasks();
//	PRINT("%d tasks\n", uxArraySize);
	if (uxArraySize > 32) {
		PRINT("too many tasks to print\n");
		return true;
	}

	uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, &pulTotalRunTime);

	PRINT("id name             S B  P  counter   Stk base stack remaining\n");
	PRINT("-- ---------------- - -- -- --------- -------- ---------------\n");

	for (i=0; i<uxArraySize; i++) {
		TaskStatus_t* pTask = &pxTaskStatusArray[i];
		char	cState = ' ';
		switch (pTask->eCurrentState) {
			case eRunning:		cState = 'x';	break;
			case eReady:		cState = 'r';	break;
			case eBlocked:		cState = 'b';	break;
			case eSuspended:	cState = 's';	break;
			case eDeleted:		cState = 'd';	break;
			case eInvalid:		cState = 'n';	break;
			default:
				cState = ' ';	break;

		}

		PRINT("%2d %-16s ", pTask->xTaskNumber, pTask->pcTaskName);
		PRINT("%c %2d %2d %9d ", cState, pTask->uxCurrentPriority, pTask->uxBasePriority, (uint32_t)pTask->ulRunTimeCounter);
		PRINT("%08x ", pTask->pxStackBase);
    	PRINT("%d\n", pTask->usStackHighWaterMark);
	}

	return true;
}

DEBUG_MENU_START(g_menu)
	DEBUG_MENU_CMD("ver",			NULL,		NULL, dbgVer)
	DEBUG_MENU_CMD("ps",			NULL,		NULL, dbgPs)
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

    ret = xTaskCreate(_task, "cli", 8192, NULL, 8, NULL);
    if (ret != pdPASS) {
        //ERROR
        return false;
    }
    return true;
}