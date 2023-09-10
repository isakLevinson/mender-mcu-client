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
    uint32_t    id;
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
    pBuffer->id = g_buf.id;
    pBuffer->len = 0;
    pBuffer->cs = 0;

    g_buf.id++;

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

bool BUFFER_rewind(uint32_t id)
{
//    INFO("BUFFER_rewind %d\n", id);
    int tail = g_buf.tail;

//    INFO("#0 %d %d \n", g_buf.bufers[g_buf.tail].id, id);

    if (tail > 0) {
        tail--;
    } else {
        tail = MAXIMAL_PROCESSING_BUFFERS_COUNT - 1;
    }

    while (g_buf.bufers[tail].id > id) {
        if (tail > 0) {
            tail--;
        } else {
            tail = MAXIMAL_PROCESSING_BUFFERS_COUNT - 1;
        }
//        INFO("#1\n");
        g_buf.count++;
    }

    tail++;
    if (tail >= MAXIMAL_PROCESSING_BUFFERS_COUNT) {
        tail = 0;
    }
    g_buf.tail = tail;

    INFO("after: h:%d, t:%d, c:%d\n", g_buf.head, g_buf.tail, g_buf.count);

    return true;
}

static bool dbgRewind(uint8_t argc, char **argv)
{
    int     count;

    if (argc < 2) {
        return false;
    }

    count = strtol(argv[1], NULL, 10);
    BUFFER_rewind(count);

    return true;
}

bool BUFFERS_addBuf(BUFFER* i_pBuf, void* i_pData, uint16_t size)
{
    int i;
    uint8_t* pBuf = i_pBuf->buf + i_pBuf->len;
    uint8_t* pData = i_pData;

    if (i_pBuf->len + size >= PROCESSING_BUFFER_MAX_SIZE) {
        return false;
    }

    for (i=0; i<size; i++) {
        *pBuf = *pData;
        i_pBuf->cs += *pData;
        pBuf++;
        pData++;
    }

	i_pBuf->len += size;

	return true;
}

bool BUFFERS_addByte(BUFFER* i_pBuf, uint8_t data)
{
    return BUFFERS_addBuf(i_pBuf, &data, 1);
}

uint8_t BUFFERS_getCs(BUFFER* i_pBuf)
{
    return i_pBuf->cs;
}


static bool dbgStatus(uint8_t argc, char **argv)
{
    int i;
 
    PRINT("max   %d\n", g_buf.maxCount);
    PRINT("head  %d\n", g_buf.head);
    PRINT("tail  %d\n", g_buf.tail);
    PRINT("count %d\n", g_buf.count);

    i = g_buf.tail - 10;
    if (i<0) {
        i += MAXIMAL_PROCESSING_BUFFERS_COUNT;
    }

    while (i != g_buf.head) {
        PRINT("%d: %d\n", i, g_buf.bufers[i].id);

        i++;
        if (i >= MAXIMAL_PROCESSING_BUFFERS_COUNT)  {
            i =0;
        }
    }

    return true;
}

// *INDENT-OFF*
DEBUG_MENU_START(g_menu)
	DEBUG_MENU_DIR("buffers", NULL)
		DEBUG_MENU_CMD("status",	NULL,		NULL, dbgStatus)
		DEBUG_MENU_CMD("rewind",    NULL,		NULL, dbgRewind)
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*

bool BUFFER_init(void)
{
	DBG_TREE_add("/", g_menu);

    _init();

    return true;
}

