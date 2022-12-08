/* Iperf Example - wifi commands

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

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

/* BLE */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "bleprph.h"



static const char *TAG = "cmd_ble";


void initialise_ble(void)
{
    nimble_port_init();
}


static int ble_cmd_status(int argc, char **argv)
{
    BaseType_t ret;

    ESP_LOGI(TAG, "ble status");

    return 0;
}

void register_ble(void)
{

    const esp_console_cmd_t listener_cmd = {
        .command = "ble",
        .help = "ble status",
        .hint = NULL,
        .func = &ble_cmd_status,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&listener_cmd) );

}