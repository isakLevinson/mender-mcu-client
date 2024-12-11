
#define DEF_DBG_MODULE	DBG_MODULE_APP

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include "driver/spi_master.h"
#include "spi_led.h"


static spi_device_handle_t g_spi;


void    SPILED_setRgb(uint8_t* rgb)
{
	int ret;
	spi_transaction_t t = {0};
	uint8_t bit = 0;
	uint8_t buf[9];
	uint8_t i;

	for (i = 0; i < 24; i++) {
		if (GET_BIT(rgb[i / 8], i % 8)) {
			BIT_SET(buf[bit / 8], bit % 8);
			bit++;
			BIT_SET(buf[bit / 8], bit % 8);
			bit++;
			bit++;
		} else {
			BIT_SET(buf[bit / 8], bit % 8);
			bit++;
			bit++;
			bit++;
		}
	}

	t.length = 8 * 9;
	t.tx_buffer = buf;

	ret = spi_device_polling_transmit(g_spi, &t);

	if (ESP_OK != ret) {
		ERROR("spi_device_polling_transmit %d\n", ret);
	}
}


static bool dbgSet(uint8_t argc, char** argv)
{
	uint8_t rgb[3];

	rgb[0] = strtol(argv[1], NULL, 10);
	rgb[1] = strtol(argv[2], NULL, 10);
	rgb[2] = strtol(argv[3], NULL, 10);

	SPILED_setRgb(rgb);

	return true;
}

DEBUG_MENU_START(g_menu)
DEBUG_MENU_DIR("led", NULL)
DEBUG_MENU_CMD("set",	NULL,		NULL, dbgSet)
DEBUG_MENU_DIR_END
DEBUG_MENU_END

void    SPILED_init(void)
{
	int ret;

	spi_bus_config_t buscfg = {
		//        .miso_io_num = PIN_NUM_MISO,
		.mosi_io_num = GPIO_LED,
		//        .sclk_io_num = PIN_NUM_CLK,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.max_transfer_sz = 80
	};

	spi_device_interface_config_t devcfg = {
		.clock_speed_hz = 2500 * 1000,     //Clock out at 2.5M MHz
		.mode = 0,                              //SPI mode 0
		//        .spics_io_num = PIN_NUM_CS,             //CS pin
		.queue_size = 3,                        //We want to be able to queue 3 transactions at a time
		//        .pre_cb = lcd_spi_pre_transfer_callback, //Specify pre-transfer callback to handle D/C line
	};

	ret = spi_bus_initialize(LED_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
	ret = spi_bus_add_device(LED_SPI_HOST, &devcfg, &g_spi);

	DBG_TREE_add("/",		g_menu);

}

