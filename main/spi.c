
#define DEF_DBG_MODULE	DBG_MODULE_ADC

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_timer.h"

#include "esp_system.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "main.h"
#include "cli.h"
#include "spi.h"

#define PIN_NUM_CLK_0  1
#define PIN_NUM_MOSI_0 2
#define PIN_NUM_MISO_0 3

#define PIN_NUM_CLK_1  4
#define PIN_NUM_MOSI_1 5
#define PIN_NUM_MISO_1 6

static const spi_host_device_t _spiHosts[2] = {
    SPI2_HOST,
    SPI3_HOST,
};

static const uint8_t _csPins[2][8] = {
    {7,   8,  9, 10, 11, 12, 13, 14}, // channel 0
    {15, 16, 17, 18, 35, 36, 37, 38}, // channel 1
};

static EventGroupHandle_t _event_group;
esp_timer_handle_t periodic_timer;

DMA_ATTR uint8_t     txBuf0[1024] = {0};
DMA_ATTR uint8_t     rxBuf0[1024] = {0};
DMA_ATTR uint8_t     txBuf1[1024] = {0};
DMA_ATTR uint8_t     rxBuf1[1024] = {0};

typedef struct {
    uint8_t dev;
    uint8_t remainingTrans;
} USER_TRANSACTION;

static USER_TRANSACTION     g_transUser[2];
static spi_transaction_t    transaction[2][2];


static void _spi_pre_transfer_callback(spi_transaction_t *t)
{
//    INFO("lcd_spi_pre_transfer_callback\n");
}

static void _spi_post_transfer_callback(spi_transaction_t *t)
{
    USER_TRANSACTION*   pUser = (USER_TRANSACTION*)t;

    if (pUser->remainingTrans) {
        xEventGroupSetBits(_event_group, 1 << pUser->dev);
    }
}

static spi_device_handle_t spi_dev[2];

static bool _init(void)
{
    esp_err_t ret;

    spi_bus_config_t buscfg0 = {
        .miso_io_num    = PIN_NUM_MISO_0,
        .mosi_io_num    = PIN_NUM_MOSI_0,
        .sclk_io_num    = PIN_NUM_CLK_0,
        .quadwp_io_num  = -1,
        .quadhd_io_num  = -1,
        .max_transfer_sz= 1024,
    };

    spi_bus_config_t buscfg1 = {
        .miso_io_num    = PIN_NUM_MISO_1,
        .mosi_io_num    = PIN_NUM_MOSI_1,
        .sclk_io_num    = PIN_NUM_CLK_1,
        .quadwp_io_num  = -1,
        .quadhd_io_num  = -1,
        .max_transfer_sz= 1024,
    };

    ret = spi_bus_initialize(_spiHosts[0], &buscfg0, SPI_DMA_CH_AUTO); // SPI_DMA_DISABLED
    //ret = spi_bus_initialize(ADC_HOST0, &buscfg0, SPI_DMA_DISABLED); // 
    if (ESP_OK != ret) {
        ERROR("spi_bus_initialize0 %x\n", ret);
    }

    ret = spi_bus_initialize(_spiHosts[1], &buscfg1, SPI_DMA_CH_AUTO);
    //ret = spi_bus_initialize(ADC_HOST1, &buscfg1, SPI_DMA_DISABLED);
    if (ESP_OK != ret) {
        ERROR("spi_bus_initialize1 %x\n", ret);
    }

   _event_group = xEventGroupCreate();

    return true;
}

static bool _addDevice(uint8_t dev, uint8_t ch)
{
    esp_err_t   ret;

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz=10*1000*1000,          // Clock out at 10 MHz
        .mode=0,                               // SPI mode 0
        .queue_size = 7,                       // We want to be able to queue 7 transactions at a time
        .pre_cb = _spi_pre_transfer_callback,  // Specify pre-transfer callback to handle D/C line
        .post_cb = _spi_post_transfer_callback,
    };

    devcfg.spics_io_num = _csPins[dev][ch];

    ret = spi_bus_add_device(_spiHosts[dev], &devcfg, &spi_dev[dev]);
    if (ESP_OK != ret) {
        ERROR("spi_bus_add_device %d:%d %x\n", dev, ch, ret);
        return false;
    }

    return true;
}

bool    SPI_txAsync(uint8_t dev, uint8_t ch, void* txBuf, size_t txSize)
{
    esp_err_t   ret;

    if (dev >= 2) {
        return false;
    }

    if (ch >= 8) {
        return false;
    }

    _addDevice(dev, ch);

    transaction[dev][0].length = 8 * txSize;
    transaction[dev][0].rxlength = 0;
    transaction[dev][0].flags = 0;
    transaction[dev][0].tx_buffer = txBuf;
    transaction[dev][0].rx_buffer = NULL;
    transaction[dev][0].user = &g_transUser[0];
    g_transUser[0].dev = dev;
    g_transUser[0].remainingTrans = 0;

    ret = spi_device_acquire_bus(spi_dev[dev], portMAX_DELAY);
    if (ESP_OK != ret) {
        ERROR("spi_device_acquire_bus %d %x\n", dev, ret);
        return false;
    }

    ret = spi_device_queue_trans(spi_dev[dev], &transaction[dev][0], portMAX_DELAY);
    if (ESP_OK != ret) {
        ERROR("spi_device_queue_trans %d:%d %x\n", dev, ch, ret);
        return false;
    }

    return true;
}

bool    SPI_txrxAsync(uint8_t dev, uint8_t ch, void* txBuf, size_t txSize, void* rxBuf, size_t rxSize)
{
    esp_err_t   ret;

    if (dev >= 2) {
        return false;
    }

    if (ch >= 8) {
        return false;
    }

    _addDevice(dev, ch);

    transaction[dev][0].length = 8 * txSize;
    transaction[dev][0].rxlength = 0;
    transaction[dev][0].flags = SPI_TRANS_CS_KEEP_ACTIVE;
    transaction[dev][0].tx_buffer = txBuf;
    transaction[dev][0].rx_buffer = NULL;
    transaction[dev][0].user = &g_transUser[0];
    g_transUser[0].dev = dev;
    g_transUser[0].remainingTrans = 1;


    transaction[dev][1].length = 8 * rxSize;
    transaction[dev][1].rxlength = 8 * rxSize;
    transaction[dev][1].flags = 0;//SPI_TRANS_USE_TXDATA;
    transaction[dev][1].tx_buffer = txBuf1;
    transaction[dev][1].rx_buffer = rxBuf;
    transaction[dev][1].user = (void*)(1<<dev);
    transaction[dev][1].user = &g_transUser[1];
    g_transUser[1].dev = dev;
    g_transUser[1].remainingTrans = 0;

    ret = spi_device_acquire_bus(spi_dev[dev], portMAX_DELAY);
    if (ESP_OK != ret) {
        ERROR("spi_device_acquire_bus %d %x\n", dev, ret);
        return false;
    }

    ret = spi_device_queue_trans(spi_dev[dev], &transaction[dev][0], portMAX_DELAY);
    if (ESP_OK != ret) {
        ERROR("spi_device_queue_trans %d:%d %x\n", dev, ch, ret);
        return false;
    }

    ret = spi_device_queue_trans(spi_dev[dev], &transaction[dev][1], portMAX_DELAY);
    if (ESP_OK != ret) {
        ERROR("spi_device_queue_trans %d:%d %x\n", dev, ch, ret);
        return false;
    }

    return true;
}

bool    SPI_waitForCompletion(uint8_t dev, int timeout)
{
    esp_err_t   ret;
    spi_transaction_t*  pTransaction;
    USER_TRANSACTION*   pUser;

    int bits = xEventGroupWaitBits(_event_group, (1<<dev), 1, 1, timeout);

    if (!(bits & (1<<dev))) {
        ERROR("wait error %x %x\n", bits, 1<<dev);
    }

    do {
        ret = spi_device_get_trans_result(spi_dev[dev], &pTransaction, portMAX_DELAY);
        if (ESP_OK != ret) {
            ERROR("spi_device_get_trans_result %d %x\n", dev, ret);
            return false;
        }

        pUser = (USER_TRANSACTION*)pTransaction->user;
    } while (pUser->remainingTrans);

    spi_device_release_bus(spi_dev[dev]);

    ret = spi_bus_remove_device(spi_dev[dev]);
    if (ESP_OK != ret) {
        ERROR("spi_bus_remove_device %d %x\n", dev, ret);
        return false;
    }

    return true;
}

bool    SPI_txrx(uint8_t dev, uint8_t ch, void* txBuf, size_t txSize, void* rxBuf, size_t rxSize)
{
    bool    ret;

    ret = SPI_txrxAsync(dev, ch, txBuf, txSize, rxBuf, rxSize);
    ret = SPI_waitForCompletion(dev, portMAX_DELAY);

    return true;
}

static bool dbgStatus(uint8_t argc, char** argv)
{
    PRINT("status\n");
    return true;
}

static bool dbgTx(uint8_t argc, char** argv)
{
    bool       ret;
    int i;
    //int bits;
    int64_t time_start;
    int64_t time_1;
    int64_t time_2;
    int64_t time_end;
    uint8_t  dev = 0;
    uint16_t   txSize;

    if (argc < 3) {
        return false;
    }

    dev = strtoul(argv[1], NULL, 10);
    txSize = strtoul(argv[2], NULL, 10);

    for (i=0; i<txSize; i++) {
        txBuf0[i] = i;
        txBuf1[i] = 3*i;
    }

    time_start = esp_timer_get_time();

    if (dev & 1) {
        ret = SPI_txAsync(0, 0, txBuf0, txSize);
        if (!ret) {
            PRINT("SPI_txAsync 0 ailed\n");
            return true;
        }
    }

    time_1 = esp_timer_get_time();

    if (dev & 2) {
        ret = SPI_txAsync(1, 0, txBuf1, txSize);
        if (!ret) {
            PRINT("SPI_txAsync 1 failed\n");
            return true;
        }
    }

    time_2 = esp_timer_get_time();

    txSize = MIN(txSize, 16);

    if (dev & 1) {
        SPI_waitForCompletion(0, portMAX_DELAY);
    }

    if (dev & 2) {
        SPI_waitForCompletion(1, portMAX_DELAY);
    }

    time_end = esp_timer_get_time();

    PRINT("timer: %lld %lld %lld\n", time_1 - time_start, time_2 - time_start, time_end - time_start);

    return true;
}


static bool dbgTxRx(uint8_t argc, char** argv)
{
    uint8_t  dev = 0;
    uint16_t   txSize;
    uint16_t   rxSize;

    if (argc < 4) {
        return false;
    }

    dev = strtoul(argv[1], NULL, 10);

    DBG_PRINT_hex2buf(argv[2], txBuf0, &txSize);
    rxSize = strtoul(argv[3], NULL, 10);

    SPI_txrxAsync(dev, 0, txBuf0, txSize, rxBuf0, rxSize);
    SPI_waitForCompletion(dev, portMAX_DELAY);

    PRINT_BUF("rx", PRINT_BUF_STYLE_HEX_NL, rxBuf0, txSize);

    return true;
}

static bool dbgWait(uint8_t argc, char** argv)
{
    uint8_t  dev = 0;

    if (argc < 2) {
        return false;
    }

    dev = strtoul(argv[1], NULL, 10);

    SPI_waitForCompletion(dev, 100);

    return true;
}

DEBUG_MENU_START(g_menu)
    DEBUG_MENU_DIR("spi", NULL)
	    DEBUG_MENU_CMD("status",	NULL,      NULL, dbgStatus)
	    DEBUG_MENU_CMD("tx",	    NULL,      NULL, dbgTx)
	    DEBUG_MENU_CMD("txrx",	    NULL,      NULL, dbgTxRx)
	    DEBUG_MENU_CMD("wait",	    NULL,      NULL, dbgWait)
   DEBUG_MENU_DIR_END
DEBUG_MENU_END


void SPI_init(void)
{
    DBG_TREE_add("/", g_menu);

    _init();
}