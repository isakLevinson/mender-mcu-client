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
#include "time.h"
#include "cmd.h"

static struct {
    TaskHandle_t        hTaskFiller;
    TaskHandle_t        hTaskSender;
    esp_timer_handle_t  timer;
    uint32_t            count;
    uint32_t            totalSent;

    uint8_t             activeModules[SPI_DEVICES];
    
    uint32_t            sentId;
    uint32_t            ackedId;

    bool    isSim;
} g_measure;

static void _fill1(BUFFER* i_pBuf, uint8_t data)
{
	BUFFERS_addByte(i_pBuf, data);
}

static void _fill2(BUFFER* i_pBuf, uint16_t data)
{
	BUFFERS_addBuf(i_pBuf, &data, 2);
}

static void _fill4(BUFFER* i_pBuf, uint32_t data)
{
	BUFFERS_addBuf(i_pBuf, &data, 4);
}

static void _fill8(BUFFER* i_pBuf, uint64_t data)
{
	BUFFERS_addBuf(i_pBuf, &data, 8);
}

static void _fillBuffer(BUFFER* i_pBuf, void* i_pData, uint8_t len)
{
	BUFFERS_addBuf(i_pBuf, i_pData, len);
}

static void _fillCs(BUFFER* i_pBuf)
{
    uint8_t cs;

    cs = BUFFERS_getCs(i_pBuf);
    _fill1(i_pBuf, cs);
}

static void _fillHeader(BUFFER* i_pBuf, COMM_TYPE type, uint8_t len)
{
    int64_t    time;
	// Get Current Meassure Time Stamp In (MicroSeconds)
	TIME_get64(&time);

	// Fill Message Header Data Into Outgoing Buffer ( 1 + 1  + 4 + 4 + 1) == 11 Bytes
	_fill1(i_pBuf, START_MESSAGE_CHARACTER);    // Start Of Message Character
	_fill1(i_pBuf, (uint8_t)type);  // Message Type
	_fill4(i_pBuf, i_pBuf->id);     // Message Refernce Number
	_fill8(i_pBuf, time);           // Device Current TimeStamp In Microseconds
	_fill1(i_pBuf, len);            // Message Length
}

#define MODULE_VALUES_ARRAY_MAX_SIZE_OLD (1+(16+2)*3)
static void _fillAdcModule(BUFFER* i_pBuf, uint8_t* i_pData, uint8_t module)
{
    uint8_t dummy[12] = {0};

	// Fill Probe Data Message ( 11 + 1 + 54 + 1) == 67 Bytes
	//_fillHeader	( i_pBuf, COMM_TYPE_DATA,	(1  + MODULE_VALUES_ARRAY_MAX_SIZE_OLD));
    _fillHeader	(i_pBuf, COMM_TYPE_DATA,	(1+15+12+15+12));
	_fill1		(i_pBuf, module);
	_fillBuffer	(i_pBuf, i_pData,      15);
	_fillBuffer (i_pBuf, dummy,        12);
	_fillBuffer (i_pBuf, i_pData+15,   15);
	_fillBuffer (i_pBuf, dummy,        12);
	_fillCs		(i_pBuf);
}

/*
static void _fillAuxData(BUFFER* i_pBuf, LSM6DSO_READINGS* i_pData, ADC_DATA* i_pAucData)
{

	int totalSize = sizeof(LSM6DSO_READINGS) + 4;

	_fillHeader (i_pBuf, COMM_TYPE_IMU,      totalSize);
	_fillBuffer (i_pBuf, (uint8_t*)i_pData,	sizeof(LSM6DSO_READINGS));
	_fill2      (i_pBuf, i_pAucData->gsr);
	_fill2      (i_pBuf, i_pAucData->ecg);
	_fillCs		(i_pBuf);
}
*/

static void _fillStartOfSession(BUFFER* i_pBuf)
{
	_fillHeader (i_pBuf, COMM_TYPE_START_SESSION, 0);
	_fillCs		(i_pBuf);
}

static bool _isModuleActive(uint8_t spi, uint8_t module)
{
    if (spi >= SPI_DEVICES) {
        return false;
    }

    if (module >= MODULES_PER_SPI) {
        return false;
    }

    return IS_BIT_SET(g_measure.activeModules[spi], module);
}

static void _fillData(BUFFER* i_pBuf)
{
    bool ret;
    uint8_t i;
    uint8_t j;
    uint8_t rxBuf[SPI_DEVICES][30];

	for (i = 0; i < MODULES_PER_SPI; i++) {
        for (j=0; j<SPI_DEVICES; j++) {
            if (_isModuleActive(j, i)) {
                ret = ADS1299_dataRd(i, j, rxBuf[i], 30);
            }
        }

        for (j=0; j<SPI_DEVICES; j++) {
            if (_isModuleActive(j, i)) {
                ret = SPI_waitForCompletion(i, portMAX_DELAY);
            	_fillAdcModule(i_pBuf, rxBuf[i], (i<<2) | (j<<4));
            }
        }
    }
}

static void _taskFillter(void *arg)
{
    uint32_t	event;
    BUFFER*     pBuffer;
    int         i;

    while (true) {
        event = xTaskNotifyWait(0, 0x01, NULL, 5000);

        if (event) {
            //TRACE("# %x\n", event);

            pBuffer = BUFFER_getHead();
            if (!pBuffer) {
                continue;
            }

            int32_t t;

            t = TIME_get32();

            pBuffer->len = 512;

            memset(pBuffer->buf, '#', pBuffer->len);
            pBuffer->buf[pBuffer->len-1] = '\n';

            sprintf((char*)pBuffer->buf, "#%d, %d bytes, %dmS Bps:%d ", g_measure.count, g_measure.totalSent, t/1000, g_measure.totalSent*1000 / (t/1000));

            g_measure.count++;
            g_measure.totalSent += pBuffer->len;

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
        event = xTaskNotifyWait(0, 0x01, NULL, 1000);

        //if (event) {
            //TRACE("## %x\n", event);
            do {
                int d;

                d = (int)g_measure.sentId - (int)g_measure.ackedId;

                //if (d > 10) {
                //    INFO("rewinding to %d from %d\n", g_measure.ackedId, g_measure.sentId);
                //    BUFFER_rewind(g_measure.ackedId);
                //}

                pBuffer = BUFFER_getTail();
                if (!pBuffer) {
                    continue;
                }

                TRACE("send (len=%d) id=%d, ack=%d\n", pBuffer->len, pBuffer->id, pBuffer->id - g_measure.ackedId);

                SER_sendTcp(pBuffer->buf, pBuffer->len);

                g_measure.sentId = pBuffer->id;

                BUFFER_pop();
            } while (pBuffer);
        //}
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

    ret = xTaskCreate(_taskFillter, "fillter", 4096, NULL, 4, &g_measure.hTaskFiller);
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

bool MEASURE_udpAck(int id, int count)
{
    INFO("ack %d %d\n", id, count);

    //if (count == g_measure.ackedId+1) {
/*        
    if (true) {
        if (d>1) {
            INFO("ack %d %d *\n", count, d);
        } else {
            TRACE("ack %d %d\n", count, d);
        }
    }
*/
    g_measure.ackedId = count;
    return true;
}

bool MEASURE_start(int interval, bool isSim)
{
    bool    ret;

    g_measure.isSim     = isSim;
    g_measure.count     = 0;
    g_measure.totalSent = 0;
    g_measure.ackedId   = 0;
    g_measure.sentId    = 0;

    TIME_set64(0);

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

