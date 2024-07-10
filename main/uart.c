
#define DEF_DBG_MODULE	DBG_MODULE_CMD

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "argtable3/argtable3.h"
#include "freertos/task.h"
#include "driver/uart.h"

#include "uart.h"
#include "cmd.h"

static bool _cbSend(int socket, COMM_TYPE type, void* i_pBuf, uint16_t size)
{
    TRACE("TX s:%d t:%d ", size, type);
    TRACE_BUF("buf",	PRINT_BUF_STYLE_HEX_SIZE_NL, i_pBuf, size);
    uart_write_bytes(UART_PORT_NUM_CMD, &size, 2);
    uart_write_bytes(UART_PORT_NUM_CMD, &type, 1);
    uart_write_bytes(UART_PORT_NUM_CMD, i_pBuf, size);
    return true;
}

static CMD_CONTEXT g_cmdContext = {
    .p_cbSend = _cbSend,
};


static void _task(void *arg)
{
    uint8_t buf[256];
    uint16_t    len = 0;
    uint16_t    i;
    bool        resetDone = false;

    while (true) {
        len = uart_read_bytes(UART_PORT_NUM_CMD, buf, sizeof(buf)-1, 100 / portTICK_PERIOD_MS);

        if (len) {
            //TRACE_BUF("rx",	PRINT_BUF_STYLE_HEX_SIZE_NL, buf, len);
            //uart_write_bytes(UART_PORT_NUM_CMD, buf, len);

            for (i=0; i<len; i++) {
                CMD_parseByte(&g_cmdContext, buf[i]);
            }
            resetDone = false;
        } else {
            if (!resetDone) {
                CMD_parseInit();
                resetDone = true;
            }
        }
    }
}


static void _init(void)
{
    int ret;
    
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_CMD,
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

    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM_CMD, 1024 * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM_CMD, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM_CMD, GPIO_UART_TXD, GPIO_UART_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

//    ESP_ERROR_CHECK(uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
//    ESP_ERROR_CHECK(uart_param_config(CONFIG_ESP_CONSOLE_UART_NUM, &uart_config));
//    ESP_ERROR_CHECK(uart_set_pin(CONFIG_ESP_CONSOLE_UART_NUM, -1, -1, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));


//        int len = uart_read_bytes(UART_PORT_NUM_CMD, data, (BUF_SIZE - 1), 20 / portTICK_PERIOD_MS);
//        uart_write_bytes(UART_PORT_NUM_CMD, (const char *) data, len);


    ret = xTaskCreate(_task, "app", 8192, NULL, 3, NULL);
    if (ret != pdPASS) {
        ERROR("create task %s failed\n", "app");
        return;
    }
}


void UART_init(void)
{
    _init();
}