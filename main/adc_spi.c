
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

#define ADC_HOST    SPI2_HOST

#define PIN_NUM_CLK  4
#define PIN_NUM_MOSI 5
#define PIN_NUM_MISO 6
#define PIN_NUM_CS   7

static EventGroupHandle_t _event_group;
esp_timer_handle_t periodic_timer;

static void _spi_pre_transfer_callback(spi_transaction_t *t)
{
    INFO("lcd_spi_pre_transfer_callback\n");
//    int dc=(int)t->user;
//    gpio_set_level(PIN_NUM_DC, dc);
}

static void _spi_post_transfer_callback(spi_transaction_t *t)
{
    //INFO("_spi_post_transfer_callback\n");
}


spi_device_handle_t spi_dev0;

static bool _init(void)
{
    esp_err_t ret;

    spi_bus_config_t buscfg={
        .miso_io_num=PIN_NUM_MISO,
        .mosi_io_num=PIN_NUM_MOSI,
        .sclk_io_num=PIN_NUM_CLK,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
        .max_transfer_sz=1024,
    };

    spi_device_interface_config_t devcfg={
        .clock_speed_hz=10*1000*1000,          // Clock out at 10 MHz
        .mode=0,                               // SPI mode 0
        .spics_io_num=PIN_NUM_CS,              // CS pin
        .queue_size = 7,                       // We want to be able to queue 7 transactions at a time
        .pre_cb = _spi_pre_transfer_callback,  // Specify pre-transfer callback to handle D/C line
        .post_cb = _spi_post_transfer_callback,
    };

    ret = spi_bus_initialize(ADC_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ESP_OK != ret) {
        ERROR("spi_bus_initialize %x\n", ret);
    }

    ret = spi_bus_add_device(ADC_HOST, &devcfg, &spi_dev0);
    if (ESP_OK != ret) {
        ERROR("spi_bus_add_device %x\n", ret);
    }

    return true;
}

static bool dbgStatus(uint8_t argc, char** argv)
{
    PRINT("status\n");
    return true;
}

static bool dbgInit(uint8_t argc, char** argv)
{
    esp_err_t ret;

    spi_bus_config_t buscfg = {
        .sclk_io_num=PIN_NUM_CLK,
        .mosi_io_num=PIN_NUM_MOSI,
        .miso_io_num=PIN_NUM_MISO,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
        .max_transfer_sz=2048,
    };

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz=10*1000*1000,           //Clock out at 10 MHz
        .mode=0,                                //SPI mode 0
        .spics_io_num=PIN_NUM_CS,               //CS pin
        .queue_size=7,                          //We want to be able to queue 7 transactions at a time
        .pre_cb = _spi_pre_transfer_callback,
        .post_cb = _spi_post_transfer_callback,
    };

    if (argc < 5) {
        return false;
    }

    buscfg.sclk_io_num  = strtoul(argv[1], NULL, 10);
    buscfg.mosi_io_num  = strtoul(argv[2], NULL, 10);
    buscfg.miso_io_num  = strtoul(argv[3], NULL, 10);
    devcfg.spics_io_num = strtoul(argv[4], NULL, 10);

    INFO("calling spi_bus_initialize\n");

    //ret = spi_bus_initialize(ADC_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ret = spi_bus_initialize(ADC_HOST, &buscfg, SPI_DMA_DISABLED);
    if (ESP_OK != ret) {
        ERROR("spi_bus_initialize %x\n", ret);
        return true;
    }

    ret = spi_bus_add_device(ADC_HOST, &devcfg, &spi_dev0);
    if (ESP_OK != ret) {
        ERROR("spi_bus_add_device %x\n", ret);
        return false;
    }

    return true;
}

static bool dbgUninit(uint8_t argc, char** argv)
{
    esp_err_t ret;

    ret = spi_bus_remove_device(spi_dev0);
    if (ESP_OK != ret) {
        ERROR("spi_bus_remove_device %x\n", ret);
        return true;
    }

    ret = spi_bus_free(ADC_HOST);
    if (ESP_OK != ret) {
        ERROR("spi_bus_free %x\n", ret);
        return true;
    }

    return true;
}

static uint8_t     txBuf[1024];
static uint8_t     rxBuf[1024];
static spi_transaction_t t;

static bool dbgTx(uint8_t argc, char** argv)
{
    uint16_t    size;
    esp_err_t   ret;
    int i;

    if (argc < 2) {
        return false;
    }

    //DBG_PRINT_hex2buf(argv[1], txbuf, &size);
    size = strtoul(argv[1], NULL, 10);
    for (i=0; i<size; i++) {
        txBuf[i] = 3*i;
    }

    t.length = 8 * size;
    t.rxlength = 8 * size;
    t.flags = 0;//SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA;
    t.user = NULL;
    t.tx_buffer = txBuf;
    t.rx_buffer = rxBuf;
    //t.tx_data[0] = 0x12;

/*
    spi_device_acquire_bus(spi_dev0, portMAX_DELAY);

    ret = spi_device_polling_transmit(spi_dev0, &t);
    if (ESP_OK != ret) {
        ERROR("spi_device_polling_transmit %x\n", ret);
        return true;
    }

    spi_device_release_bus(spi_dev0);
*/

    //ret = spi_device_transmit(spi_dev0, &t);
    ret = spi_device_queue_trans(spi_dev0, &t, portMAX_DELAY);


    if (ESP_OK != ret) {
        ERROR("spi_device_transmit %x\n", ret);
        return true;
    }

    size = MIN(size, 16);
    PRINT_BUF(NULL, PRINT_BUF_STYLE_HEX_NL, rxBuf, size);

    return true;
}

DEBUG_MENU_START(g_menu)
    DEBUG_MENU_DIR("adc", NULL)
	    DEBUG_MENU_CMD("status",	NULL,      NULL, dbgStatus)
	    DEBUG_MENU_CMD("tx",	    NULL,      NULL, dbgTx)
	    DEBUG_MENU_CMD("init",	    NULL,      NULL, dbgInit)
	    DEBUG_MENU_CMD("uninit",	NULL,      NULL, dbgUninit)
    DEBUG_MENU_DIR_END
DEBUG_MENU_END


void ADCSPI_init(void)
{
    DBG_TREE_add("/", g_menu);

   _init();
}