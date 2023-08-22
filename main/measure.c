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
#include "wifi.h"
#include "ads1299.h"
#include "measure.h"
#include "buffer.h"

static struct {
    TaskHandle_t        hTaskFiller;
    TaskHandle_t        hTaskSender;
    esp_timer_handle_t  timer;

    bool    isSim;

} g_measure;

static void _taskFillter(void *arg)
{
    uint32_t	event;
    BUFFER*     pBuffer;
    int         count = 0;
    int         i;

    while (true) {
        event = xTaskNotifyWait(0, 0x01, NULL, 5000);

        if (event) {
            //TRACE("# %x\n", event);

            pBuffer = BUFFER_getHead();
            if (!pBuffer) {
                continue;
            }

            pBuffer->len = 0;
            pBuffer->len += sprintf((char*)pBuffer->buf+pBuffer->len, "%d, ", count++);

            for (i=0; i<2000; i++) {
                pBuffer->len += sprintf((char*)pBuffer->buf+pBuffer->len, "#");
            }
            pBuffer->len += sprintf((char*)pBuffer->buf+pBuffer->len, "\n");

            BUFFER_push();

            xTaskNotify(g_measure.hTaskSender, 1, eSetBits);
        }
    }
}

static void _taskSender(void *arg)
{
    uint32_t	event;
    BUFFER*     pBuffer;

    while (true) {
        event = xTaskNotifyWait(0, 0x01, NULL, 5000);

        if (event) {
            //TRACE("## %x\n", event);
            do {
                pBuffer = BUFFER_getTail();
                if (!pBuffer) {
                    continue;
                }

                TRACE("send (%d)[%s]\n", pBuffer->len, pBuffer->buf);

                SER_sendUdp(pBuffer->buf, pBuffer->len);

                BUFFER_pop();
            } while (pBuffer);
        }
    }
}

static void _timerCb(void* arg)
{
    xTaskNotify(g_measure.hTaskFiller, 1, eSetBits);
}

static void _init(void)
{
    int ret;

    const esp_timer_create_args_t timer_args = {
        .callback = &_timerCb,
        .name = "periodic"
    };

    ret = esp_timer_create(&timer_args, &g_measure.timer);
    if (ESP_OK != ret) {
        ERROR("esp_timer_create %d\n", ret);
    }

    ret = xTaskCreate(_taskFillter, "fillter", 4096, NULL, 3, &g_measure.hTaskFiller);
    if (ret != pdPASS) {
        ERROR("create task %s failed\n", "filler");
        return;
    }

    ret = xTaskCreate(_taskSender, "sender", 4096, NULL, 3, &g_measure.hTaskSender);
    if (ret != pdPASS) {
        ERROR("create task %s failed\n", "sender");
        return;
    }
}

bool MEASURE_start(int interval, bool isSim)
{
    bool    ret;

    g_measure.isSim = isSim;

    if (isSim) {
        ret = esp_timer_start_periodic(g_measure.timer, interval * 1000);
        if (ESP_OK != ret) {
            ERROR("esp_timer_start_periodic %d\n", ret);
        }
    }

    return true;
}

bool MEASURE_stop(void)
{
    if (g_measure.isSim) {
        esp_timer_stop(g_measure.timer);
    }

    return true;
}

static bool dbgStatus(uint8_t argc, char **argv)
{
    return true;
}

static bool dbgStart(uint8_t argc, char **argv)
{
    bool    ret;
    int     interval;

    if (argc < 2) {
        MEASURE_stop();
        return true;
    }

    interval = strtol(argv[1], NULL, 10);
    MEASURE_start(interval, true);

    return true;
}

static bool dbgTx(uint8_t argc, char **argv)
{
    uint8_t     buf[64];
    uint16_t    size = sizeof(buf);

    DBG_PRINT_hex2buf(argv[1], buf, &size);

    SER_sendUdp(buf, size);

    return true;
}


// *INDENT-OFF*
DEBUG_MENU_START(g_menu)
	DEBUG_MENU_DIR("measure", NULL)
		DEBUG_MENU_CMD("status",	NULL,		NULL, dbgStatus)
		DEBUG_MENU_CMD("start",	    NULL,		NULL, dbgStart)
		DEBUG_MENU_CMD("tx",	    NULL,		NULL, dbgTx)
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*


bool MEASURE_init(void)
{
    int ret;

	DBG_TREE_add("/", g_menu);

    _init();

    return true;
}

