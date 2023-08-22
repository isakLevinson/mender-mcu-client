/* Wi-Fi iperf Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/


#define DEF_DBG_MODULE	DBG_MODULE_MEASURE

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include <errno.h>
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "cmd_ble.h"
#include "main.h"
#include "cli.h"
#include "spi.h"
#include "cmd.h"
#include "ads1299.h"
#include "buffer.h"

static struct {
    BUFFER      bufers[MAXIMAL_PROCESSING_BUFFERS_COUNT];
    uint16_t    head;
    uint16_t    tail;
    uint16_t    count;
    uint16_t    maxCount;
} g_buf;

static void _init(void)
{
    g_buf.head      = 0;
    g_buf.tail      = 0;
    g_buf.count     = 0;
    g_buf.maxCount  = 0;
}

BUFFER* BUFFER_getHead(void)
{
    BUFFER* pBuffer;

    if (g_buf.count >= MAXIMAL_PROCESSING_BUFFERS_COUNT) {
        return NULL;
    }

    pBuffer = &g_buf.bufers[g_buf.head];
    pBuffer->len = 0;
    return  pBuffer;
}

BUFFER* BUFFER_getTail(void)
{
    BUFFER* pBuffer;

    if (!g_buf.count) {
        return NULL;
    }

    pBuffer = &g_buf.bufers[g_buf.tail];
    return  pBuffer;
}

bool BUFFER_push(void)
{
    if (g_buf.count >= MAXIMAL_PROCESSING_BUFFERS_COUNT) {
        return false;
    }

    g_buf.count++;
    if (g_buf.count > g_buf.maxCount) {
        g_buf.maxCount = g_buf.count;
    }

    g_buf.head++;
    if (g_buf.head >= MAXIMAL_PROCESSING_BUFFERS_COUNT) {
        g_buf.head = 0;
    }
    return true;
}

bool BUFFER_pop(void)
{
    if (!g_buf.count) {
        return false;
    }

    g_buf.tail++;
    if (g_buf.tail >= MAXIMAL_PROCESSING_BUFFERS_COUNT) {
        g_buf.tail = 0;
    }
    g_buf.count--;
    return true;
}

static bool dbgStatus(uint8_t argc, char **argv)
{
    PRINT("max %d\n", g_buf.maxCount);
    return true;
}

// *INDENT-OFF*
DEBUG_MENU_START(g_menu)
	DEBUG_MENU_DIR("buffers", NULL)
		DEBUG_MENU_CMD("status",	NULL,		NULL, dbgStatus)
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*

bool BUFFER_init(void)
{
	DBG_TREE_add("/", g_menu);

    _init();

    return true;
}

