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
#include "measure.h"

static struct {
    TaskHandle_t        pxCreatedTask;
    esp_timer_handle_t  timer;

} g_measure;

static void _task(void *arg)
{
    uint32_t	event;

    while (true) {
        //event = osSignalWait(1, 500);
        event = xTaskNotifyWait(0, 0x01, NULL, 5000);

        if (event) {
            TRACE("## %x\n", event);
        }
    }
}

static void _timerCb(void* arg)
{
    xTaskNotify(g_measure.pxCreatedTask, 1, eSetBits);
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

    ret = xTaskCreate(_task, "measure", 4096, NULL, 3, &g_measure.pxCreatedTask);
    if (ret != pdPASS) {
        ERROR("create task %s failed\n", "measure");
        return;
    }
}

static bool dbgStatus(uint8_t argc, char **argv)
{
    return true;
}

static bool dbgTrig(uint8_t argc, char **argv)
{
    xTaskNotify( g_measure.pxCreatedTask, 1, eSetBits);

    return true;
}

static bool dbgStart(uint8_t argc, char **argv)
{
    bool    ret;
    int     interval;

    if (argc < 2) {
        esp_timer_stop(g_measure.timer);
        return true;
    }

    interval = strtol(argv[1], NULL, 10);

    ret = esp_timer_start_periodic(g_measure.timer, interval * 1000);
    if (ESP_OK != ret) {
        ERROR("esp_timer_start_periodic %d\n", ret);
    }

    return true;
}


// *INDENT-OFF*
DEBUG_MENU_START(g_menu)
	DEBUG_MENU_DIR("measure", NULL)
		DEBUG_MENU_CMD("status",	NULL,		NULL, dbgStatus)
		DEBUG_MENU_CMD("trig",	    NULL,		NULL, dbgTrig)
		DEBUG_MENU_CMD("start",	    NULL,		NULL, dbgStart)
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

