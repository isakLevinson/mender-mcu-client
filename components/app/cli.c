

#define DEF_DBG_MODULE	DBG_MODULE_MAIN

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/uart.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "esp_partition.h"
#include "config.h"

#include "main.h"
#include "cli.h"
#include "fifo.h"
#include "time.h"
#include "mender_ota.h"

static struct {
	DBG_DECODE_INST		decoder;
	SemaphoreHandle_t	mutex;
	TaskStatus_t 		taskStatusArray[32];

#if USE_FLASH_LOG
	const esp_partition_t* logPartition;

	FIFO			logRamFifo;
	FIFO			logFlashFifo;

	uint8_t			logBuf[2048];
	uint32_t		erasedSector;
#endif
} g_cli;

static uint32_t _mutexGet(uint8_t devMask)
{
	xSemaphoreTake(g_cli.mutex, portMAX_DELAY);

	return true;
}

static bool _mutexPost(uint8_t devMask, uint32_t state)
{
	xSemaphoreGive(g_cli.mutex);

	return true;
}

static bool _puts(uint8_t devBitmap, char* i_pStr, uint16_t size)
{
	bool	retVal;
	char	decodedBuf[256];
	uint16_t	decodedSize;

	if (devBitmap & DBG_OUT_STREAM_DEVICE_MASK_CLI) {
		retVal = DBG_PRINT_decode(&g_cli.decoder, (uint8_t*)i_pStr, size, decodedBuf, &decodedSize, NULL);
		if (retVal) {
			uart_write_bytes(CONFIG_ESP_CONSOLE_UART_NUM, decodedBuf, decodedSize);
		}
	}

#if USE_FLASH_LOG
	if (devBitmap & DBG_OUT_STREAM_DEVICE_MASK_LOG) {
		FIFO_push(&g_cli.logRamFifo, i_pStr, size);
	}
#endif

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
	int64_t t;

	TIME_get64(&t);

	return t / 1000;
}

#if USE_FLASH_LOG
static void _flashRead(void* pArg, uint32_t addr, uint8_t* o_pData, uint16_t size)
{
	esp_err_t	err;

	if (!g_cli.logPartition) {
		return;
	}

	err =  esp_partition_read(g_cli.logPartition, addr, o_pData, size);
	if (ESP_OK != err) {
		ERROR("esp_partition_read %s\n", ESP_getErrStr(err));
	}
}

static void _flashWrite(void* pArg, uint32_t addr, uint8_t* i_pData, uint16_t size)
{
	esp_err_t	err;

	if (!g_cli.logPartition) {
		return;
	}

	const uint32_t	sectorSize = g_cli.logPartition->erase_size;
	const uint32_t	eraseMask = sectorSize - 1;

	if (g_cli.erasedSector != ((addr + size) & ~eraseMask)) {
		g_cli.erasedSector = ((addr + size) & ~eraseMask);

		err = esp_partition_erase_range(g_cli.logPartition, g_cli.erasedSector, sectorSize);
		if (ESP_OK != err) {
			ERROR("esp_partition_erase_range %s\n", ESP_getErrStr(err));
		}
	}

	err = esp_partition_write(g_cli.logPartition, addr, i_pData, size);
	if (ESP_OK != err) {
		ERROR("esp_partition_write %s\n", ESP_getErrStr(err));
	}
}

static bool _logInit(void)
{
	g_cli.logPartition  = esp_partition_find_first(0x40, 2, NULL);

	if (!g_cli.logPartition) {
		return false;
	}

	FIFO_CONFIG   configRamLog = {
		.type			= FIFO_MSG_TYPE_STREAM,
		.pBuf			= g_cli.logBuf,
		.size			= sizeof(g_cli.logBuf),
		.popOldOnFull	= true,
		.pName			= "logRam",
	};

	FIFO_CONFIG   configFlashLog = {
		.type			= FIFO_MSG_TYPE_STREAM,
		.size			= 0,//QSPI_PARTITION_LOG_END - QSPI_PARTITION_LOG_START,
		.pCbRd			= _flashRead,
		.pCbWr			= _flashWrite,
		.popOldOnFull	= true,
		.erasePopped	= true,
		.reservedSpace	= 16,
		.pName			= "logFlash",
	};

	configFlashLog.size = g_cli.logPartition->size;

	FIFO_init(&g_cli.logRamFifo,	&configRamLog);
	FIFO_init(&g_cli.logFlashFifo,	&configFlashLog);

	return true;
}
#endif

static void _taskCli(void* arg)
{
	char c;
	size_t length;

	PRINT("Ready.\n");

	while (true) {
		length = uart_read_bytes(CONFIG_ESP_CONSOLE_UART_NUM, &c, 1, 1);
		if (length) {
			DBG_MENU_handler(c);
		}
	}

	vTaskDelete(NULL);
}

static void _taskLog(void* arg)
{
	uint16_t	popedSize;
	uint8_t		buf[512];

	while (true) {
		vTaskDelay(10);
		//		INFO("calling FIFO_peekLast\n");
		popedSize = FIFO_peekLast(&g_cli.logRamFifo, buf, sizeof(buf), NULL);
		//		INFO("popedSize: %d\n", popedSize);

#if 1
		if (popedSize < 256) {
			continue;
		}
		while (popedSize > 0) {
			popedSize &= 0xfffc;

			if (!popedSize) {
				continue;;
			}

			FIFO_push(&g_cli.logFlashFifo, buf, popedSize);
			FIFO_pop(&g_cli.logRamFifo, NULL, popedSize);

			popedSize = FIFO_peekLast(&g_cli.logRamFifo, buf, sizeof(buf), NULL);
		}
#endif
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
	bool	ret;
	int		err;
	char*	proj;
	char*	ver;
	char	hw_ver[64] = {0};
	char	sn[64];

	MENDER_version(&proj, &ver);
	ret = CFG_get(cfg_id_sn, sn, sizeof(sn));
	if (ret) {
		PRINT("sn: %s\n", sn);
	} else {
		PRINT("SN not set\n");
	}

	PRINT("proj: %s\n", proj);
	PRINT("sw: %s\n", ver);
	ret = CFG_get(cfg_id_hw_revision, hw_ver, sizeof(hw_ver));
	if (ret) {
		PRINT("hw: %s\n", hw_ver);
	} else {
		PRINT("HW version not set\n");
	}

#if 0
	uint8_t mac[6];
	err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
	if (err) {
		ERROR("esp_read_mac failed %d\n", err);
	} else {
		PRINT("default MAC %02x%02x%02x%02x%02x%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	}
#endif
	return true;
}

static void _printDiffs(char* prefix, int old, int new)
{
	if (old != new) {
		PRINT("%s %d -> %d (%d)\n", prefix, old, new, new - old);
	} else {
		PRINT("%s %d\n", prefix, new);
	}
}

static bool dbgPs(uint8_t argc, char** argv)
{
	UBaseType_t	uxArraySize;
	TaskStatus_t taskStatusArray[32] = {0};
	bool		 valid[32] = {0};
	bool		 stackReduced[32] = {0};
	uint32_t	 dt[32] = {0};
	uint8_t		i;
	unsigned long pulTotalRunTime;

	uxArraySize = uxTaskGetNumberOfTasks();
	//	PRINT("%d tasks\n", uxArraySize);
	if (uxArraySize > 32) {
		PRINT("too many tasks to print\n");
		return true;
	}

	uxTaskGetSystemState(taskStatusArray, uxArraySize, &pulTotalRunTime);

	for (i = 0; i < uxArraySize; i++) {
		TaskStatus_t* pTask = &taskStatusArray[i];
		valid[pTask->xTaskNumber] = true;
		dt[pTask->xTaskNumber] = pTask->ulRunTimeCounter - g_cli.taskStatusArray[pTask->xTaskNumber].ulRunTimeCounter;

		if (pTask->usStackHighWaterMark < g_cli.taskStatusArray[pTask->xTaskNumber].usStackHighWaterMark) {
			stackReduced[pTask->xTaskNumber] = true;
		}
		memcpy(&g_cli.taskStatusArray[pTask->xTaskNumber], pTask, sizeof(g_cli.taskStatusArray[0]));
	}

	PRINT("id name             S B  P  counter   Stk base stack remaining\n");
	PRINT("-- ---------------- - -- -- --------- -------- ---------------\n");

	for (i = 0; i < 32; i++) {
		if (!valid[i]) {
			memset(&g_cli.taskStatusArray[i], 0, sizeof(g_cli.taskStatusArray[0]));
		}
		if (!g_cli.taskStatusArray[i].xTaskNumber) {
			continue;
		}

		TaskStatus_t* pTask = &g_cli.taskStatusArray[i];

		char	cState = ' ';
		switch (pTask->eCurrentState) {
			case eRunning:
				cState = 'x';
				break;
			case eReady:
				cState = 'r';
				break;
			case eBlocked:
				cState = 'b';
				break;
			case eSuspended:
				cState = 's';
				break;
			case eDeleted:
				cState = 'd';
				break;
			case eInvalid:
				cState = 'n';
				break;
			default:
				cState = ' ';
				break;
		}

		PRINT("%2d %-16s ", pTask->xTaskNumber, pTask->pcTaskName);
		PRINT("%c %2d %2d %9d ", cState, pTask->uxCurrentPriority, pTask->uxBasePriority, dt[i]);
		PRINT("%08x ", pTask->pxStackBase);
		PRINT("%d", pTask->usStackHighWaterMark);
		if (stackReduced[i]) {
			PRINT("*");
		}
		PRINT("\n");
	}

	multi_heap_info_t heap_info;
	size_t internal_ram_free	= heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
	size_t spi_ram_free			= heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

	static multi_heap_info_t heap_hist = {0};

	heap_caps_get_info(&heap_info, MALLOC_CAP_DEFAULT);

	PRINT("\n");
	PRINT("SPI RAM free         : %d\n", spi_ram_free);
	PRINT("Internal RAM free    : %d\n", internal_ram_free);

	_printDiffs("Total free bytes     :", heap_hist.total_free_bytes, heap_info.total_free_bytes);
	_printDiffs("Total allocated bytes:", heap_hist.total_allocated_bytes, heap_info.total_allocated_bytes);
	_printDiffs("Largest free block   :", heap_hist.largest_free_block, heap_info.largest_free_block);
	_printDiffs("Free blocks          :", heap_hist.free_blocks, heap_info.free_blocks);
	_printDiffs("Allocated blocks     :", heap_hist.allocated_blocks, heap_info.allocated_blocks);

	memcpy(&heap_hist, &heap_info, sizeof(heap_hist));

	return true;
}

static bool dbgLogStatus(uint8_t argc, char** argv)
{
	if (!g_cli.logPartition) {
		PRINT("failed\n");
		return true;
	}

	PRINT("log partition '%s' chip:%x offset:%x size:%x, erase_size:%x\n",
	    g_cli.logPartition->label,
	    g_cli.logPartition->flash_chip,
	    g_cli.logPartition->address,
	    g_cli.logPartition->size,
	    g_cli.logPartition->erase_size);

	return true;
}

static bool dbgLogClear(uint8_t argc, char** argv)
{
	esp_err_t	err;

	if (!g_cli.logPartition) {
		PRINT("failed\n");
		return true;
	}

	err = esp_partition_erase_range(g_cli.logPartition, 0, g_cli.logPartition->size);
	if (ESP_OK != err) {
		PRINT("erase failed %d\n", err);
	}

	FIFO_clear(&g_cli.logFlashFifo);
	FIFO_clear(&g_cli.logRamFifo);

	return true;
}

static bool dbgLogRecover(uint8_t argc, char** argv)
{
	FIFO_recoverPointers(&g_cli.logFlashFifo);

	return true;
}

static bool dbgLogRead(uint8_t argc, char** argv)
{
	uint32_t	addr;
	uint32_t	size = 16;
	uint8_t		buf[256];

	if (argc < 2) {
		return false;
	}

	addr	= strtoul(argv[1], NULL, 16);

	if (argc >= 3) {
		size = strtoul(argv[2], NULL, 16);
	}

	_flashRead(NULL, addr, buf, size);
	PRINT_BUF(NULL, PRINT_BUF_STYLE_HEX_SIZE_NL, buf, size);

	return true;
}

static bool dbgLogWrite(uint8_t argc, char** argv)
{
	uint32_t	addr;
	uint8_t		buf[256];
	uint16_t	size = sizeof(buf);

	if (argc < 3) {
		return false;
	}

	addr	= strtoul(argv[1], NULL, 16);
	DBG_PRINT_hex2buf(argv[2], buf, &size);

	_flashWrite(NULL, addr, buf, size);

	return true;
}

static bool dbgLogErase(uint8_t argc, char** argv)
{
	esp_err_t	err;
	uint32_t	addr;

	if (argc < 2) {
		return false;
	}

	addr	= strtoul(argv[1], NULL, 16);

	PRINT("erasing addr:%x, size:%x\n", addr, g_cli.logPartition->erase_size);

	err = esp_partition_erase_range(g_cli.logPartition, addr, g_cli.logPartition->erase_size);
	if (ESP_OK != err) {
		PRINT("failed %d\n", err);
	}

	return true;
}

static bool dbgLogTail(uint8_t argc, char** argv)
{
	bool			ret;
	uint8_t			buf[64];
	char			decodedBuf[256];
	uint16_t		size;
	uint16_t		decodedSize;
	DBG_DECODE_INST	decoder;

	uint32_t	totalPopped = 0;
	uint32_t	totalDecoded = 0;

	int32_t	loc = 1000;

	DBG_PRINT_decodeInit(&decoder);

	size = FIFO_peekLast(&g_cli.logFlashFifo, buf, sizeof(buf), NULL);

	if (argc >= 2) {
		loc = strtoul(argv[1], NULL, 10);
	}

	PRINT("printing last %d bytes\n", loc);
	FIFO_peekSetLocation(&g_cli.logFlashFifo, -loc);

	while (size) {
		ret = DBG_PRINT_decode(&decoder, buf, size, decodedBuf, &decodedSize, NULL);
		totalPopped		+= size;
		totalDecoded	+= decodedSize;
		if (ret) {
			uart_write_bytes(CONFIG_ESP_CONSOLE_UART_NUM, decodedBuf, decodedSize);

		}
		size = FIFO_peekNext(&g_cli.logFlashFifo, buf, sizeof(buf), NULL);
	}

	PRINT("\ntotal popped %d decoded %d\n", totalPopped, totalDecoded);
	return true;
}

static bool dbgTime(uint8_t argc, char** argv)
{
	int64_t		t;
	int32_t		t32;
	char		str[64];

	TIME_get64(&t);
	t32 = TIME_get32();

	uint32_t	us = t % 1000000;
	t /= 1000000;

	TIME_strftime(t, "%Y-%m-%d %H:%M:%S", str);
	PRINT("time from start: %dmS\n", t32);
	PRINT("%d.%06d\n", (int32_t)t, us);
	PRINT("%s\n", str);

	return true;
}

// *INDENT-OFF*
DEBUG_MENU_START(g_menu)
	DEBUG_MENU_CMD("ver",	NULL,	NULL, dbgVer)
	DEBUG_MENU_CMD("ps",	NULL,	NULL, dbgPs)
	DEBUG_MENU_CMD("tail",	NULL,	NULL, dbgLogTail)
	DEBUG_MENU_CMD("time",	NULL,	NULL, dbgTime)
	DEBUG_MENU_DIR("log", NULL)
		DEBUG_MENU_CMD("status",	NULL,		NULL, dbgLogStatus)
		DEBUG_MENU_CMD("clear",		NULL,		NULL, dbgLogClear)
		DEBUG_MENU_CMD("recover",	NULL,		NULL, dbgLogRecover)
		DEBUG_MENU_CMD("r",			NULL,		NULL, dbgLogRead)
		DEBUG_MENU_CMD("w",			NULL,		NULL, dbgLogWrite)
		DEBUG_MENU_CMD("e",			NULL,		NULL, dbgLogErase)
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*

bool	CLI_init(void)
{
	DBG_PRINT_CONFIG	dbgPrintCfg = {
		.cbPuts				= _puts,
		.cbMutexGet			= _mutexGet,
		.cbMutexRelease		= _mutexPost,
		.cbGetTime64		= _getTime,
		.printTimestamp		= true,
	};

	DBG_MENU_CONFIG cfg = {
		.eol		= '\r',
		.ignore		= '\n',
		.pPrompt	= PROMPT " \\w\\$ ",
		.pfAlloc	= _alloc,
		.pfFree		= _free,
	};

	g_cli.mutex = xSemaphoreCreateMutex();

	_logInit();

	DBG_PRINT_decodeInit(&g_cli.decoder);
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

	ret = xTaskCreate(_taskCli, "cli", 8192, NULL, 8, NULL);
	if (ret != pdPASS) {
		//ERROR
		return false;
	}

	ret = xTaskCreate(_taskLog, "log", 4096, NULL, 8, NULL);
	if (ret != pdPASS) {
		//ERROR
		return false;
	}

	return true;
}