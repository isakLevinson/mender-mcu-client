
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

#define ADC_HOST0    SPI2_HOST
#define ADC_HOST1    SPI3_HOST

#define PIN_NUM_CLK_0  4
#define PIN_NUM_MOSI_0 5
#define PIN_NUM_MISO_0 6
#define PIN_NUM_CS_0   7

#define PIN_NUM_CLK_1  8
#define PIN_NUM_MOSI_1 9
#define PIN_NUM_MISO_1 10
#define PIN_NUM_CS_1   11

static EventGroupHandle_t _event_group;
esp_timer_handle_t periodic_timer;

DMA_ATTR uint8_t     txBuf0[1024];
DMA_ATTR uint8_t     rxBuf0[1024];
DMA_ATTR uint8_t     txBuf1[1024];
DMA_ATTR uint8_t     rxBuf1[1024];
static spi_transaction_t t0;
static spi_transaction_t t1;


static void _spi_pre_transfer_callback(spi_transaction_t *t)
{
//    INFO("lcd_spi_pre_transfer_callback\n");
}

static void _spi_post_transfer_callback(spi_transaction_t *t)
{
    uint8_t dev = (uint8_t)t->user;
    //INFO("_spi_post_transfer_callback\n");
    xEventGroupSetBits(_event_group, dev);
}


spi_device_handle_t spi_dev0;
spi_device_handle_t spi_dev1;

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

    spi_device_interface_config_t devcfg0 = {
        .clock_speed_hz=10*1000*1000,          // Clock out at 10 MHz
        .mode=0,                               // SPI mode 0
        .spics_io_num=PIN_NUM_CS_0,              // CS pin
        .queue_size = 7,                       // We want to be able to queue 7 transactions at a time
        .pre_cb = _spi_pre_transfer_callback,  // Specify pre-transfer callback to handle D/C line
        .post_cb = _spi_post_transfer_callback,
    };

    spi_device_interface_config_t devcfg1 = {
        .clock_speed_hz=10*1000*1000,          // Clock out at 10 MHz
        .mode=0,                               // SPI mode 0
        .spics_io_num=PIN_NUM_CS_1,              // CS pin
        .queue_size = 7,                       // We want to be able to queue 7 transactions at a time
        .pre_cb = _spi_pre_transfer_callback,  // Specify pre-transfer callback to handle D/C line
        .post_cb = _spi_post_transfer_callback,
    };

    ret = spi_bus_initialize(ADC_HOST0, &buscfg0, SPI_DMA_CH_AUTO);
    if (ESP_OK != ret) {
        ERROR("spi_bus_initialize0 %x\n", ret);
    }

    ret = spi_bus_initialize(ADC_HOST1, &buscfg1, SPI_DMA_CH_AUTO);
    if (ESP_OK != ret) {
        ERROR("spi_bus_initialize1 %x\n", ret);
    }

    ret = spi_bus_add_device(ADC_HOST0, &devcfg0, &spi_dev0);
    if (ESP_OK != ret) {
        ERROR("spi_bus_add_device %x\n", ret);
    }

    ret = spi_bus_add_device(ADC_HOST0, &devcfg1, &spi_dev1);
    if (ESP_OK != ret) {
        ERROR("spi_bus_add_device %x\n", ret);
    }

   _event_group = xEventGroupCreate();

    return true;
}

static bool dbgStatus(uint8_t argc, char** argv)
{
    PRINT("status\n");
    return true;
}

static bool dbgTx(uint8_t argc, char** argv)
{
    uint16_t    size;
    esp_err_t   ret;
    int i;
    //int bits;
    int64_t time_start;
    int64_t time_1;
    int64_t time_2;
    int64_t time_end;
    uint8_t  dev = 0;

    if (argc < 2) {
        return false;
    }

    //DBG_PRINT_hex2buf(argv[1], txbuf, &size);
    dev = strtoul(argv[1], NULL, 10);
    size = strtoul(argv[2], NULL, 10);

    for (i=0; i<size; i++) {
        txBuf0[i] = 3*i;
        txBuf1[i] = 5*i;
    }

    t0.length = 8 * size;
    t0.rxlength = 8 * size;
    t0.flags = 0;//SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA;
    t0.user = NULL;
    t0.tx_buffer = txBuf0;
    t0.rx_buffer = rxBuf0;
    t0.user = (void*)1;

    t1.length = 8 * size;
    t1.rxlength = 8 * size;
    t1.flags = 0;//SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA;
    t1.user = NULL;
    t1.tx_buffer = txBuf1;
    t1.rx_buffer = rxBuf1;
    t1.user = (void*)2;

/*
    spi_device_acquire_bus(spi_dev0, portMAX_DELAY);

    ret = spi_device_polling_transmit(spi_dev0, &t);
    if (ESP_OK != ret) {
        ERROR("spi_device_polling_transmit %x\n", ret);
        return true;
    }

    spi_device_release_bus(spi_dev0);
*/

    time_start = esp_timer_get_time();

    if (dev & 1) {
        ret = spi_device_queue_trans(spi_dev0, &t0, portMAX_DELAY);
        if (ESP_OK != ret) {
            ERROR("spi_device_transmit0 %x\n", ret);
            return true;
        }
    }

    time_1 = esp_timer_get_time();

    if (dev & 2) {
        ret = spi_device_queue_trans(spi_dev1, &t1, portMAX_DELAY);
        if (ESP_OK != ret) {
            ERROR("spi_device_transmit1 %x\n", ret);
            return true;
        }
    }

    time_2 = esp_timer_get_time();


    xEventGroupWaitBits(_event_group, dev, 1, 1, portMAX_DELAY);

    time_end = esp_timer_get_time();

    PRINT("timer: %lld %lld %lld\n", time_1 - time_start, time_2 - time_start, time_end - time_start);

    //size = MIN(size, 16);
    //PRINT_BUF(NULL, PRINT_BUF_STYLE_HEX_NL, rxBuf, size);

    return true;
}

DEBUG_MENU_START(g_menu)
    DEBUG_MENU_DIR("adc", NULL)
	    DEBUG_MENU_CMD("status",	NULL,      NULL, dbgStatus)
	    DEBUG_MENU_CMD("tx",	    NULL,      NULL, dbgTx)
   DEBUG_MENU_DIR_END
DEBUG_MENU_END


void ADCSPI_init(void)
{
    DBG_TREE_add("/", g_menu);

   _init();
}