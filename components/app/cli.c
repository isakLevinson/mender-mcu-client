

#define DEF_DBG_MODULE	DBG_MODULE_MAIN

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "cmd_decl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "iperf.h"
#include "esp_coexist.h"

#include <sys/socket.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "iperf.h"

#include "driver/uart.h"
#include "main.h"

#include "cli.h"


static void _task(void *arg)
{
    char c;
    char str[256];
    size_t length;

    strcpy(str, "\r\nCLI task Ready\r\n");
    uart_write_bytes(ECHO_UART_PORT_NUM, str, strlen(str));
    uart_write_bytes(CONFIG_ESP_CONSOLE_UART_NUM, str, strlen(str));

    while(true) {
        uart_get_buffered_data_len(CONFIG_ESP_CONSOLE_UART_NUM, &length);

        if (length > 1) {
            length = 1;
        }

        if (!length) {
            vTaskDelay(10);
        } else {
            length = uart_read_bytes(CONFIG_ESP_CONSOLE_UART_NUM, &c, length, 1);
            if (length) {
                uart_write_bytes(CONFIG_ESP_CONSOLE_UART_NUM, &c, 1);
            }
        }
    }

    vTaskDelete(NULL);





}



void CLI_init(void)
{
    int ret;
    char str[256];

    strcpy(str, "\r\nCLI Ready\r\n");
    uart_write_bytes(ECHO_UART_PORT_NUM, str, strlen(str));
    uart_write_bytes(CONFIG_ESP_CONSOLE_UART_NUM, str, strlen(str));


    ret = xTaskCreate(_task, IPERF_TRAFFIC_TASK_NAME, IPERF_TRAFFIC_TASK_STACK, NULL, IPERF_TRAFFIC_TASK_PRIORITY, NULL);
    if (ret != pdPASS) {
        //ERROR
        return;
    }

}