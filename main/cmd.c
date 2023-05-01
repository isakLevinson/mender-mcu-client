
#define DEF_DBG_MODULE	DBG_MODULE_CMD

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "argtable3/argtable3.h"

#include "esp_event.h"
#include "esp_check.h"

#include "soc/soc_caps.h"
#include "driver/gpio.h"
#include "driver/uart.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "main.h"
#include "cmd.h"
#include "cli.h"
#include "s_crc.h"


bool CMD_tx(uint8_t* i_pBuf, size_t size)
{
    uint16_t crc;

    crc = API_CRC_ccitt16(0, i_pBuf, size);

    uart_write_bytes(ECHO_UART_PORT_NUM, i_pBuf, size);
    uart_write_bytes(ECHO_UART_PORT_NUM, &crc, 2);

    return true;
}


static void _task(void *arg)
{
    char c;
    size_t length;

    PRINT("CMD Ready.\n");

    char* str="Ready\n";
    uart_write_bytes(ECHO_UART_PORT_NUM, str, strlen(str));

    while(true) {
        length = uart_read_bytes(ECHO_UART_PORT_NUM, &c, 1, 1);
        if (length) {
            TRACE("%02x\n", c);
            //TRACE("%c\n", c);
            //uart_write_bytes(ECHO_UART_PORT_NUM, &c, 1);
        }
    }

    vTaskDelete(NULL);
}

static bool _init(void)
{
    int ret;

    #if 0
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = ECHO_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    int intr_alloc_flags = 0;

#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(ECHO_UART_PORT_NUM, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS));

    // Configure a temporary buffer for the incoming data
    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);
#endif

    ret = xTaskCreate(_task, "cmd", 4096, NULL, 7, NULL);
    if (ret != pdPASS) {
        //ERROR
        return false;
    }

    return true;
}

static bool dbgStatus(uint8_t argc, char** argv)
{
    return true;
}

static bool dbgTx(uint8_t argc, char** argv)
{
	uint8_t	buf[256];
	uint16_t	size = sizeof(buf);
    //uint16_t    crc;

	if (argc < 2) {
		return false;
	}

	DBG_PRINT_hex2buf(argv[1], buf, &size);
/*
    crc = API_CRC_ccitt16(0, buf, size);
    INFO("crc=%04x\n", crc);
    buf[size] = crc & 0xff;
    buf[size+1] = crc >> 8;

    uart_write_bytes(ECHO_UART_PORT_NUM, buf, size+2);
*/
    CMD_tx(buf, size);

	return true;
}

DEBUG_MENU_START(g_menu)
    DEBUG_MENU_DIR("cmd", NULL)
	    DEBUG_MENU_CMD("status",		NULL,		NULL, dbgStatus)
	    DEBUG_MENU_CMD("tx",			NULL,		NULL, dbgTx)
    DEBUG_MENU_DIR_END
DEBUG_MENU_END


void CMD_init(void)
{
    DBG_TREE_add("/",		g_menu);

    _init();
}