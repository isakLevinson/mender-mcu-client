
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
#include "adc_spi.h"

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

DMA_ATTR uint8_t     txBuf0[1024];
DMA_ATTR uint8_t     rxBuf0[1024];
DMA_ATTR uint8_t     txBuf1[1024];
DMA_ATTR uint8_t     rxBuf1[1024];
static spi_transaction_t transaction[2];


static void _spi_pre_transfer_callback(spi_transaction_t *t)
{
//    INFO("lcd_spi_pre_transfer_callback\n");
}

static void _spi_post_transfer_callback(spi_transaction_t *t)
{
    int devMask = (int)t->user;
    //INFO("_spi_post_transfer_callback\n");

    xEventGroupSetBits(_event_group, devMask);
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

bool    SPI_txStart(uint8_t dev, uint8_t ch, void* txBuf, size_t txSize, void* rxBuf, size_t rxSize)
{
    esp_err_t   ret;

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz=10*1000*1000,          // Clock out at 10 MHz
        .mode=0,                               // SPI mode 0
        .queue_size = 7,                       // We want to be able to queue 7 transactions at a time
        .pre_cb = _spi_pre_transfer_callback,  // Specify pre-transfer callback to handle D/C line
        .post_cb = _spi_post_transfer_callback,
    };

    if (dev >= 2) {
        return false;
    }

    if (ch >= 8) {
        return false;
    }

    devcfg.spics_io_num = _csPins[dev][ch];

    ret = spi_bus_add_device(_spiHosts[dev], &devcfg, &spi_dev[dev]);
    if (ESP_OK != ret) {
        ERROR("spi_bus_add_device %d:%d %x\n", dev, ch, ret);
        return false;
    }

    transaction[dev].length = 8 * txSize;
    transaction[dev].rxlength = 8 * rxSize;
    transaction[dev].flags = 0;
    transaction[dev].tx_buffer = txBuf;
    transaction[dev].rx_buffer = rxBuf;
    transaction[dev].user = (void*)(1<<dev);

    ret = spi_device_queue_trans(spi_dev[dev], &transaction[dev], portMAX_DELAY);
    if (ESP_OK != ret) {
        ERROR("spi_device_queue_trans %d:%d %x\n", dev, ch, ret);
        return false;
    }

    return true;
}

bool    SPI_waitForCompletion(uint8_t dev)
{
    esp_err_t   ret;
    spi_transaction_t* pTransaction;

    int bits = xEventGroupWaitBits(_event_group, (1<<dev), 1, 1, portMAX_DELAY);

    if (bits != (1<<dev)) {
        ERROR("wait error %x %x\n", bits, 1<<dev);
    }

    ret = spi_device_get_trans_result(spi_dev[dev], &pTransaction, 1000);
    if (ESP_OK != ret) {
        ERROR("spi_device_get_trans_result %d %x\n", dev, ret);
        return false;
    }

    ret = spi_bus_remove_device(spi_dev[dev]);
    if (ESP_OK != ret) {
        ERROR("spi_bus_remove_device %d %x\n", dev, ret);
        return false;
    }

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
    uint16_t   rxSize;

    if (argc < 3) {
        return false;
    }

    dev = strtoul(argv[1], NULL, 10);
    txSize = strtoul(argv[2], NULL, 10);
    rxSize = txSize;

    if (argc >= 4) {
        rxSize = strtoul(argv[3], NULL, 10);
    }

    for (i=0; i<txSize; i++) {
        txBuf0[i] = i;
        txBuf1[i] = 3*i;
    }

    time_start = esp_timer_get_time();

    if (dev & 1) {
        ret = SPI_txStart(0, 0, txBuf0, txSize, rxBuf0, rxSize);
        if (!ret) {
            PRINT("SPI_txStart 0 ailed\n");
            return true;
        }
    }

    time_1 = esp_timer_get_time();

    if (dev & 2) {
        ret = SPI_txStart(1, 0, txBuf1, txSize, rxBuf1, rxSize);
        if (!ret) {
            PRINT("SPI_txStart 1 failed\n");
            return true;
        }
    }

    time_2 = esp_timer_get_time();

    txSize = MIN(txSize, 16);

    if (dev & 1) {
        SPI_waitForCompletion(0);
    }

    if (dev & 2) {
        SPI_waitForCompletion(1);
    }

    time_end = esp_timer_get_time();

    PRINT("timer: %lld %lld %lld\n", time_1 - time_start, time_2 - time_start, time_end - time_start);

    if (dev & 1) {
        PRINT_BUF("rx0", PRINT_BUF_STYLE_HEX_NL, rxBuf0, txSize);
    }

    if (dev & 2) {
        PRINT_BUF("rx1", PRINT_BUF_STYLE_HEX_NL, rxBuf1, txSize);
    }

    return true;
}

static bool dbgWait(uint8_t argc, char** argv)
{
    uint8_t  dev = 0;

    if (argc < 2) {
        return false;
    }

    //DBG_PRINT_hex2buf(argv[1], txbuf, &size);
    dev = strtoul(argv[1], NULL, 10);

    if (dev & 1) {
        SPI_waitForCompletion(0);
    }

    if (dev & 2) {
        SPI_waitForCompletion(1);
    }

    return true;
}

DEBUG_MENU_START(g_menu)
    DEBUG_MENU_DIR("adc", NULL)
	    DEBUG_MENU_CMD("status",	NULL,      NULL, dbgStatus)
	    DEBUG_MENU_CMD("tx",	    NULL,      NULL, dbgTx)
	    DEBUG_MENU_CMD("wait",	    NULL,      NULL, dbgWait)
   DEBUG_MENU_DIR_END
DEBUG_MENU_END


void ADCSPI_init(void)
{
    DBG_TREE_add("/", g_menu);

    _init();
}