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
#include "cmd_wifi.h"
#include "nvs.h"


typedef struct {
    struct arg_str *ip;
    struct arg_lit *server;
    struct arg_lit *udp;
    struct arg_lit *version;
    struct arg_int *port;
    struct arg_int *length;
    struct arg_int *interval;
    struct arg_int *time;
    struct arg_int *bw_limit;
    struct arg_lit *abort;
    struct arg_end *end;
} wifi_iperf_t;
static wifi_iperf_t iperf_args;

static struct {
    nvs_handle_t nvsHandle;
    char    currentSsid[32];
    char    currentPasswd[32];
} g_wifi;


typedef struct {
    struct arg_str *ssid;
    struct arg_str *password;
    struct arg_end *end;
} wifi_args_t;

typedef struct {
    struct arg_str *ssid;
    struct arg_end *end;
} wifi_scan_arg_t;

static wifi_args_t sta_args;
static wifi_scan_arg_t scan_args;
static wifi_args_t ap_args;
static bool reconnect = true;
static const char *TAG = "cmd_wifi";
static esp_netif_t *netif_ap = NULL;
static esp_netif_t *netif_sta = NULL;

static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;
const int DISCONNECTED_BIT = BIT1;

#define NVS_NAMESPACE_WIFI      "wifi"
#define NVS_KEY_WIFI_SSID       "ssid"
#define NVS_KEY_WIFI_PASSWD     "passwd"

bool    wifi_nvs_get_ssid(char* ssid, char* passwd)
{
    bool    ret = true;
    esp_err_t err = ESP_OK;
    nvs_handle_t handle;
    size_t length;
    char* pStr = NULL;

    err = nvs_open(NVS_NAMESPACE_WIFI, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        printf("nvs_open failed %x\n", err);
        return false;
    }

    err =  nvs_get_str(handle, NVS_KEY_WIFI_SSID, ssid, &length);
    if (err != ESP_OK) {
        printf("nvs_get_str ssid failed\n");
        ret = false;
        goto exit;
    }
    ssid[length] = '\0';

    err =  nvs_get_str(handle, NVS_KEY_WIFI_PASSWD, passwd, &length);
    if (err != ESP_OK) {
        printf("nvs_get_str passwd failed\n");
        ret = false;
        goto exit;
    }
    passwd[length] = '\0';

    exit:
    nvs_close(handle);

    switch(err) {
    case ESP_OK:    break;
    case ESP_ERR_NVS_NOT_FOUND:         pStr = "ESP_ERR_NVS_NOT_FOUND"; break;
    case ESP_ERR_NVS_NOT_INITIALIZED:   pStr = "ESP_ERR_NVS_NOT_INITIALIZED";  break;
    case ESP_ERR_NO_MEM:                pStr = "ESP_ERR_NO_MEM";  break;
    case ESP_ERR_INVALID_ARG:           pStr = "ESP_ERR_INVALID_ARG";  break;
    default:
    }

    if (pStr) {
        printf("%s\n", pStr);
    } else {
        if (ESP_OK != err) {
            printf("0x%x", err);
        }
    }

    return ret;
}

bool    wifi_nvs_set_ssid(char* ssid, char* passwd)
{
    bool    ret = true;
    esp_err_t err = ESP_OK;
    nvs_handle_t handle;

    err = nvs_open(NVS_NAMESPACE_WIFI, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        printf("nvs_open failed");
        return false;
    }

    err = nvs_set_str (handle, NVS_KEY_WIFI_SSID, ssid);
    if (err != ESP_OK) {
        printf("nvs_set_str ssid failed %x\n", err);
        ret = false;
        goto exit;
    }

    err = nvs_set_str (handle, NVS_KEY_WIFI_PASSWD, passwd);
    if (err != ESP_OK) {
        printf("nvs_set_str passwd failed %x\n", err);
        ret = false;
        goto exit;
    }

    exit:
    nvs_close(handle);
    return ret;
}

static void scan_done_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint16_t sta_number = 0;
    uint8_t i;
    wifi_ap_record_t *ap_list_buffer;

    esp_wifi_scan_get_ap_num(&sta_number);
    if (!sta_number) {
        ESP_LOGE(TAG, "No AP found");
        return;
    }

    ap_list_buffer = malloc(sta_number * sizeof(wifi_ap_record_t));
    if (ap_list_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to malloc buffer to print scan results");
        return;
    }

    if (esp_wifi_scan_get_ap_records(&sta_number, (wifi_ap_record_t *)ap_list_buffer) == ESP_OK) {
        for (i = 0; i < sta_number; i++) {
            ESP_LOGI(TAG, "[%s][rssi=%d]", ap_list_buffer[i].ssid, ap_list_buffer[i].rssi);
        }
    }
    free(ap_list_buffer);
    ESP_LOGI(TAG, "sta scan done");
}

static void got_ip_handler(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    xEventGroupClearBits(wifi_event_group, DISCONNECTED_BIT);
    xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
}

static void disconnect_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (reconnect) {
        ESP_LOGI(TAG, "sta disconnect, reconnect...");
        esp_wifi_connect();
    } else {
        ESP_LOGI(TAG, "sta disconnect");
    }
    xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
    xEventGroupSetBits(wifi_event_group, DISCONNECTED_BIT);
}

void initialise_wifi(void)
{
    esp_log_level_set("wifi", ESP_LOG_WARN);
    static bool initialized = false;

    if (initialized) {
        return;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_create_default() );
    netif_ap = esp_netif_create_default_wifi_ap();
    assert(netif_ap);
    netif_sta = esp_netif_create_default_wifi_sta();
    assert(netif_sta);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                    WIFI_EVENT_SCAN_DONE,
                    &scan_done_handler,
                    NULL,
                    NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                    WIFI_EVENT_STA_DISCONNECTED,
                    &disconnect_handler,
                    NULL,
                    NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                    IP_EVENT_STA_GOT_IP,
                    &got_ip_handler,
                    NULL,
                    NULL));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL) );
    ESP_ERROR_CHECK(esp_wifi_start() );

#if CONFIG_EXTERNAL_COEX_ENABLE
    esp_external_coex_gpio_set_t gpio_pin;
    gpio_pin.in_pin0  = 1;
    gpio_pin.in_pin1  = 2;
    gpio_pin.out_pin0 = 3;

    ESP_ERROR_CHECK( esp_enable_extern_coex_gpio_pin(EXTERN_COEX_WIRE_3, gpio_pin) );
#endif

    initialized = true;
}

bool wifi_cmd_sta_join(const char *ssid, const char *pass)
{
    int bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, 0, 1, 0);

    wifi_config_t wifi_config = { 0 };

    strlcpy((char *) wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    if (pass) {
        strlcpy((char *) wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
    }

    if (bits & CONNECTED_BIT) {
        reconnect = false;
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        ESP_ERROR_CHECK( esp_wifi_disconnect() );
        xEventGroupWaitBits(wifi_event_group, DISCONNECTED_BIT, 0, 1, portTICK_PERIOD_MS);
    }

    reconnect = true;
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    esp_wifi_connect();

    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, 0, 1, 5000 / portTICK_PERIOD_MS);

    return true;
}

static int wifi_cmd_sta(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &sta_args);

    if (nerrors != 0) {
        arg_print_errors(stderr, sta_args.end, argv[0]);
        return 1;
    }

    ESP_LOGI(TAG, "sta connecting to '%s'", sta_args.ssid->sval[0]);
    wifi_cmd_sta_join(sta_args.ssid->sval[0], sta_args.password->sval[0]);
    return 0;
}

static bool wifi_cmd_sta_scan(const char *ssid)
{
    wifi_scan_config_t scan_config = { 0 };
    scan_config.ssid = (uint8_t *) ssid;

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    esp_wifi_scan_start(&scan_config, false);

    return true;
}

static int wifi_cmd_scan(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &scan_args);

    if (nerrors != 0) {
        arg_print_errors(stderr, scan_args.end, argv[0]);
        return 1;
    }

    ESP_LOGI(TAG, "sta start to scan");
    if ( scan_args.ssid->count == 1 ) {
        wifi_cmd_sta_scan(scan_args.ssid->sval[0]);
    } else {
        wifi_cmd_sta_scan(NULL);
    }
    return 0;
}


static bool wifi_cmd_ap_set(const char *ssid, const char *pass)
{
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "",
            .ssid_len = 0,
            .max_connection = 4,
            .password = "",
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    reconnect = false;
    strlcpy((char *) wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid));
    if (pass) {
        if (strlen(pass) != 0 && strlen(pass) < 8) {
            reconnect = true;
            ESP_LOGE(TAG, "password less than 8");
            return false;
        }
        strlcpy((char *) wifi_config.ap.password, pass, sizeof(wifi_config.ap.password));
    }

    if (strlen(pass) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    return true;
}

static int wifi_cmd_ap(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &ap_args);

    if (nerrors != 0) {
        arg_print_errors(stderr, ap_args.end, argv[0]);
        return 1;
    }

    wifi_cmd_ap_set(ap_args.ssid->sval[0], ap_args.password->sval[0]);
    ESP_LOGI(TAG, "AP mode, %s %s", ap_args.ssid->sval[0], ap_args.password->sval[0]);
    return 0;
}

static int wifi_cmd_query(int argc, char **argv)
{
    wifi_config_t cfg;
    wifi_mode_t mode;
    esp_netif_ip_info_t ip;

    esp_wifi_get_mode(&mode);
    if (WIFI_MODE_AP == mode) {
        esp_wifi_get_config(WIFI_IF_AP, &cfg);
        ESP_LOGI(TAG, "AP mode, %s %s", cfg.ap.ssid, cfg.ap.password);
    } else if (WIFI_MODE_STA == mode) {
        int bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, 0, 1, 0);
        if (bits & CONNECTED_BIT) {
            esp_wifi_get_config(WIFI_IF_STA, &cfg);
            ESP_LOGI(TAG, "sta mode, connected %s", cfg.ap.ssid);
        } else {
            ESP_LOGI(TAG, "sta mode, disconnected");
        }
    } else {
        ESP_LOGI(TAG, "NULL mode");
        return 0;
    }

    memset(&ip, 0, sizeof(esp_netif_ip_info_t));

    if (esp_netif_get_ip_info(netif_sta, &ip) == 0) {
        ESP_LOGI(TAG, "IP:"IPSTR, IP2STR(&ip.ip));
        ESP_LOGI(TAG, "MASK:"IPSTR, IP2STR(&ip.netmask));
        ESP_LOGI(TAG, "GW:"IPSTR, IP2STR(&ip.gw));
    }
    return 0;
}

esp_ip4_addr_t  wifi_getSelfIp(void)
{
    esp_netif_ip_info_t ip;

    memset(&ip, 0, sizeof(esp_netif_ip_info_t));

    esp_netif_get_ip_info(netif_sta, &ip);

    return ip.ip;
}

uint8_t recvBuf[1024];
static void socket_recv(int recv_socket, struct sockaddr_storage listen_addr, uint8_t type)
{
}

static void task_listener(void *arg)
{
    esp_err_t ret;// = ESP_OK;
    int err = 0;

    esp_netif_ip_info_t ip;
    int listen_socket = -1;
    int client_socket = -1;
    struct sockaddr_in listen_addr4 = { 0 };
    struct sockaddr_storage listen_addr = { 0 };
    struct sockaddr_in remote_addr;
    //struct timeval timeout = { 0 };
    socklen_t addr_len = sizeof(struct sockaddr);
    int opt = 1;
    char    str[256];

    //int count = 0;

    ESP_LOGI(TAG, "listener task started");

    strcpy(str, "Hello");
    uart_write_bytes(ECHO_UART_PORT_NUM, str, strlen(str));

    if (esp_netif_get_ip_info(netif_sta, &ip) == 0) {
        ESP_LOGI(TAG, "IP:"IPSTR, IP2STR(&ip.ip));
        ESP_LOGI(TAG, "MASK:"IPSTR, IP2STR(&ip.netmask));
        ESP_LOGI(TAG, "GW:"IPSTR, IP2STR(&ip.gw));

        listen_addr4.sin_addr.s_addr = ip.ip.addr;

    } else {
        ESP_LOGE(TAG, "esp_netif_get_ip_info failed");
        return;
    }

    listen_addr4.sin_family = AF_INET;
    listen_addr4.sin_port = htons(7000);

    listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ESP_GOTO_ON_FALSE((listen_socket >= 0), ESP_FAIL, exit, TAG, "Unable to create socket: errno %d", errno);

    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    ESP_LOGI(TAG, "Socket created");

    err = bind(listen_socket, (struct sockaddr *)&listen_addr4, sizeof(listen_addr4));
    ESP_GOTO_ON_FALSE((err == 0), ESP_FAIL, exit, TAG, "Socket unable to bind: errno %d, IPPROTO: %d", errno, AF_INET);

    err = listen(listen_socket, 5);
    ESP_GOTO_ON_FALSE((err == 0), ESP_FAIL, exit, TAG, "Error occurred during listen: errno %d", errno);
    memcpy(&listen_addr, &listen_addr4, sizeof(listen_addr4));

   //ESP_LOGI(TAG, "listen on:"IPSTR, IP2STR(&listen_addr4));

    ESP_LOGI(TAG, "listen on addr %d.%d.%d.%d:%d",
             listen_addr4.sin_addr.s_addr & 0xFF,
             (listen_addr4.sin_addr.s_addr >> 8) & 0xFF,
             (listen_addr4.sin_addr.s_addr >> 16) & 0xFF,
             (listen_addr4.sin_addr.s_addr >> 24) & 0xFF);

    client_socket = accept(listen_socket, (struct sockaddr *)&remote_addr, &addr_len);
    ESP_GOTO_ON_FALSE((client_socket >= 0), ESP_FAIL, exit, TAG, "Unable to accept connection: errno %d", errno);
    ESP_LOGI(TAG, "accept: %s,%d\n", inet_ntoa(remote_addr.sin_addr), htons(remote_addr.sin_port));

    //timeout.tv_sec = IPERF_SOCKET_RX_TIMEOUT;
    //setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

/////////////////////////////////////////////////
    //socket_recv(client_socket, listen_addr, IPERF_TRANS_TYPE_TCP);
    uint8_t *buffer;
    int want_recv = 0;
    int actual_recv = 0;
    socklen_t socklen = sizeof(struct sockaddr_in);

    strcpy(str, "Ready\r\n");
    uart_write_bytes(ECHO_UART_PORT_NUM, str, strlen(str));

    buffer = recvBuf;
    want_recv = sizeof(recvBuf);
    //while (!s_iperf_ctrl.finish) {
    while (true) {
        actual_recv = recvfrom(client_socket, buffer, want_recv, 0, (struct sockaddr *)&listen_addr, &socklen);
        if (actual_recv < 0) {
            //iperf_show_socket_error_reason(error_log, recv_socket);
            //ESP_LOGW(TAG, "error, error code: %d, reason: %s", error_log, strerror(error_log));
            ESP_LOGW(TAG, "recv error, error code: %d", actual_recv);

            //s_iperf_ctrl.finish = true;
            break;
        } else {
            memcpy(str, buffer, actual_recv);
            str[actual_recv] = '\0';

            ESP_LOGI(TAG, "received %d <%s>", actual_recv, str);

            uart_write_bytes(ECHO_UART_PORT_NUM, buffer, actual_recv);
        }
    }


exit:
    if (client_socket != -1) {
        close(client_socket);
    }

    if (listen_socket != -1) {
        shutdown(listen_socket, 0);
        close(listen_socket);
        ESP_LOGI(TAG, "TCP Socket server is closed.");
    }
    //s_iperf_ctrl.finish = true;
    vTaskDelete(NULL);
}

static int wifi_cmd_listen(int argc, char **argv)
{
    BaseType_t ret;

    ESP_LOGI(TAG, "starting listener task");

    ret = xTaskCreatePinnedToCore(task_listener, IPERF_TRAFFIC_TASK_NAME, IPERF_TRAFFIC_TASK_STACK, NULL, IPERF_TRAFFIC_TASK_PRIORITY, NULL, portNUM_PROCESSORS - 1);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "create task %s failed", IPERF_TRAFFIC_TASK_NAME);
        return ESP_FAIL;
    }
    return 0;
}

static int wifi_cmd_nvs(int argc, char **argv)
{
    esp_err_t err = ESP_OK;

    if (argc == 2) {
        if (!strcmp(argv[1], "close")) {
            printf("close\n");
            nvs_close(g_wifi.nvsHandle);
            g_wifi.nvsHandle = NULL;
        } else if (!strcmp(argv[1], "commit")) {
            printf("commit\n");
            err = nvs_commit(g_wifi.nvsHandle);
        } else if (!strcmp(argv[1], "stats")) {
            nvs_stats_t nvs_stats;

            printf("stats\n");
            err =  nvs_get_stats(NULL, &nvs_stats);
            if (err == ESP_OK) {
                printf("used_entries   : %d\n", nvs_stats.used_entries);
                printf("free_entries   : %d\n", nvs_stats.free_entries);
                printf("total_entries  : %d\n", nvs_stats.total_entries);
                printf("namespace_count: %d\n", nvs_stats.namespace_count);
            }
        } else if (!strcmp(argv[1], "list")) {
            nvs_iterator_t it;
            printf("list\n");
            
            err =  nvs_entry_find(NVS_DEFAULT_PART_NAME, NULL, NVS_TYPE_ANY, &it);
            while (err == ESP_OK) {
                nvs_entry_info_t info;
                nvs_entry_info(it, &info); // Can omit error check if parameters are guaranteed to be non-NULL
                printf("ns: '%s', key: '%s', type: '%x' \n", info.namespace_name, info.key, info.type);

                switch (info.type) {
                    case NVS_TYPE_U8:   printf("U8");  break;
                    case NVS_TYPE_I8:   printf("I8");  break;
                    case NVS_TYPE_U16:  printf("U16");  break;
                    case NVS_TYPE_I16:  printf("I16");  break;
                    case NVS_TYPE_U32:  printf("U32");  break;
                    case NVS_TYPE_I32:  printf("I32");  break;
                    case NVS_TYPE_U64:  printf("U64");  break;
                    case NVS_TYPE_I64:  printf("I64");  break;
                    case NVS_TYPE_STR:  printf("STR");  break;
                    case NVS_TYPE_BLOB: printf("BLOB");  break;
                    default:
                }
                printf("\n");

                err = nvs_entry_next(&it);
            }
        } else if (!strcmp(argv[1], "ssid")) {
            bool ret;
            char ssid[32] = "";
            char passwd[32] = "";
            ret = wifi_nvs_get_ssid(ssid, passwd);
            if (ret) {
                printf("%s:%s\n", ssid, passwd);
            }
        }
    } else if (argc == 3) {
        if (!strcmp(argv[1], "open")) {
            printf("open %s\n", argv[2]);
            err = nvs_open(argv[2], NVS_READWRITE, &g_wifi.nvsHandle);
        } else if (!strcmp(argv[1], "get")) {
            char    str[256];
             size_t length;

            printf("get %s\n", argv[2]);
            err =  nvs_get_str(g_wifi.nvsHandle, argv[2], str, &length);
            if (err == ESP_OK) {
                str[length] = '\0';
                printf("str=<%s>\n", str);
            }
        }
    } else if (argc == 4) {
        if (!strcmp(argv[1], "set")) {
            printf("set %s <- %s\n", argv[2], argv[3]);
            err = nvs_set_str(g_wifi.nvsHandle, argv[2], argv[3]);
        } else if (!strcmp(argv[1], "ssid")) {
            wifi_nvs_set_ssid(argv[2], argv[3]);
        }
    }

    if (err != ESP_OK) {
        char* pStr = NULL;

        switch(err) {
        case ESP_ERR_NVS_NOT_FOUND:         pStr = "ESP_ERR_NVS_NOT_FOUND"; break;
        case ESP_ERR_NVS_NOT_INITIALIZED:   pStr = "ESP_ERR_NVS_NOT_INITIALIZED";  break;
        case ESP_ERR_NO_MEM:                pStr = "ESP_ERR_NO_MEM";  break;
        case ESP_ERR_INVALID_ARG:           pStr = "ESP_ERR_INVALID_ARG";  break;
        }

        if (pStr) {
            printf("failed %s\n", pStr);
        } else {
            printf("failed 0x%x\n", err);
        }
    }

    return 0;
}

static uint32_t wifi_get_local_ip(void)
{
    int bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, 0, 1, 0);
    esp_netif_t *netif = netif_ap;
    esp_netif_ip_info_t ip_info;
    wifi_mode_t mode;

    esp_wifi_get_mode(&mode);
    if (WIFI_MODE_STA == mode) {
        bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, 0, 1, 0);
        if (bits & CONNECTED_BIT) {
            netif = netif_sta;
        } else {
            ESP_LOGE(TAG, "sta has no IP");
            return 0;
        }
    }

    esp_netif_get_ip_info(netif, &ip_info);
    return ip_info.ip.addr;
}

static int wifi_cmd_iperf(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &iperf_args);
    iperf_cfg_t cfg;

    if (nerrors != 0) {
        arg_print_errors(stderr, iperf_args.end, argv[0]);
        return 0;
    }

    memset(&cfg, 0, sizeof(cfg));

    // now wifi iperf only support IPV4 address
    cfg.type = IPERF_IP_TYPE_IPV4;

    if ( iperf_args.abort->count != 0) {
        iperf_stop();
        return 0;
    }

    if ( ((iperf_args.ip->count == 0) && (iperf_args.server->count == 0)) ||
            ((iperf_args.ip->count != 0) && (iperf_args.server->count != 0)) ) {
        ESP_LOGE(TAG, "should specific client/server mode");
        return 0;
    }

    if (iperf_args.ip->count == 0) {
        cfg.flag |= IPERF_FLAG_SERVER;
    } else {
        cfg.destination_ip4 = esp_ip4addr_aton(iperf_args.ip->sval[0]);
        cfg.flag |= IPERF_FLAG_CLIENT;
    }

    cfg.source_ip4 = wifi_get_local_ip();
    if (cfg.source_ip4 == 0) {
        return 0;
    }

    if (iperf_args.udp->count == 0) {
        cfg.flag |= IPERF_FLAG_TCP;
    } else {
        cfg.flag |= IPERF_FLAG_UDP;
    }

    if (iperf_args.length->count == 0) {
        cfg.len_send_buf = 0;
    } else {
        cfg.len_send_buf = iperf_args.length->ival[0];
    }

    if (iperf_args.port->count == 0) {
        cfg.sport = IPERF_DEFAULT_PORT;
        cfg.dport = IPERF_DEFAULT_PORT;
    } else {
        if (cfg.flag & IPERF_FLAG_SERVER) {
            cfg.sport = iperf_args.port->ival[0];
            cfg.dport = IPERF_DEFAULT_PORT;
        } else {
            cfg.sport = IPERF_DEFAULT_PORT;
            cfg.dport = iperf_args.port->ival[0];
        }
    }

    if (iperf_args.interval->count == 0) {
        cfg.interval = IPERF_DEFAULT_INTERVAL;
    } else {
        cfg.interval = iperf_args.interval->ival[0];
        if (cfg.interval <= 0) {
            cfg.interval = IPERF_DEFAULT_INTERVAL;
        }
    }

    if (iperf_args.time->count == 0) {
        cfg.time = IPERF_DEFAULT_TIME;
    } else {
        cfg.time = iperf_args.time->ival[0];
        if (cfg.time <= cfg.interval) {
            cfg.time = cfg.interval;
        }
    }

    /* iperf -b */
    if (iperf_args.bw_limit->count == 0) {
        cfg.bw_lim = IPERF_DEFAULT_NO_BW_LIMIT;
    } else {
        cfg.bw_lim = iperf_args.bw_limit->ival[0];
        if (cfg.bw_lim <= 0) {
            cfg.bw_lim = IPERF_DEFAULT_NO_BW_LIMIT;
        }
    }

    ESP_LOGI(TAG, "mode=%s-%s sip=%d.%d.%d.%d:%d, dip=%d.%d.%d.%d:%d, interval=%d, time=%d",
             cfg.flag & IPERF_FLAG_TCP ? "tcp" : "udp",
             cfg.flag & IPERF_FLAG_SERVER ? "server" : "client",
             cfg.source_ip4 & 0xFF, (cfg.source_ip4 >> 8) & 0xFF, (cfg.source_ip4 >> 16) & 0xFF,
             (cfg.source_ip4 >> 24) & 0xFF, cfg.sport,
             cfg.destination_ip4 & 0xFF, (cfg.destination_ip4 >> 8) & 0xFF,
             (cfg.destination_ip4 >> 16) & 0xFF, (cfg.destination_ip4 >> 24) & 0xFF, cfg.dport,
             cfg.interval, cfg.time);

    iperf_start(&cfg);

    return 0;
}

void register_wifi(void)
{
    sta_args.ssid = arg_str1(NULL, NULL, "<ssid>", "SSID of AP");
    sta_args.password = arg_str0(NULL, NULL, "<pass>", "password of AP");
    sta_args.end = arg_end(2);

    const esp_console_cmd_t sta_cmd = {
        .command = "sta",
        .help = "WiFi is station mode, join specified soft-AP",
        .hint = NULL,
        .func = &wifi_cmd_sta,
        .argtable = &sta_args
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&sta_cmd) );

    scan_args.ssid = arg_str0(NULL, NULL, "<ssid>", "SSID of AP want to be scanned");
    scan_args.end = arg_end(1);

    const esp_console_cmd_t scan_cmd = {
        .command = "scan",
        .help = "WiFi is station mode, start scan ap",
        .hint = NULL,
        .func = &wifi_cmd_scan,
        .argtable = &scan_args
    };

    ap_args.ssid = arg_str1(NULL, NULL, "<ssid>", "SSID of AP");
    ap_args.password = arg_str0(NULL, NULL, "<pass>", "password of AP");
    ap_args.end = arg_end(2);


    ESP_ERROR_CHECK( esp_console_cmd_register(&scan_cmd) );

    const esp_console_cmd_t ap_cmd = {
        .command = "ap",
        .help = "AP mode, configure ssid and password",
        .hint = NULL,
        .func = &wifi_cmd_ap,
        .argtable = &ap_args
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&ap_cmd) );

    const esp_console_cmd_t query_cmd = {
        .command = "query",
        .help = "query WiFi info",
        .hint = NULL,
        .func = &wifi_cmd_query,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&query_cmd) );

    const esp_console_cmd_t listener_cmd = {
        .command = "listen",
        .help = "start listener task",
        .hint = NULL,
        .func = &wifi_cmd_listen,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&listener_cmd) );

    const esp_console_cmd_t nvs_cmd = {
        .command = "nvs",
        .help = "<key> [value]",
        .hint = NULL,
        .func = &wifi_cmd_nvs,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&nvs_cmd) );


    iperf_args.ip = arg_str0("c", "client", "<ip>", "run in client mode, connecting to <host>");
    iperf_args.server = arg_lit0("s", "server", "run in server mode");
    iperf_args.udp = arg_lit0("u", "udp", "use UDP rather than TCP");
    iperf_args.version = arg_lit0("V", "ipv6_domain", "use IPV6 address rather than IPV4");
    iperf_args.port = arg_int0("p", "port", "<port>", "server port to listen on/connect to");
    iperf_args.length = arg_int0("l", "len", "<length>", "Set read/write buffer size");
    iperf_args.interval = arg_int0("i", "interval", "<interval>", "seconds between periodic bandwidth reports");
    iperf_args.time = arg_int0("t", "time", "<time>", "time in seconds to transmit for (default 10 secs)");
    iperf_args.bw_limit = arg_int0("b", "bandwidth", "<bandwidth>", "bandwidth to send at in Mbits/sec");
    iperf_args.abort = arg_lit0("a", "abort", "abort running iperf");
    iperf_args.end = arg_end(1);
    const esp_console_cmd_t iperf_cmd = {
        .command = "iperf",
        .help = "iperf command",
        .hint = NULL,
        .func = &wifi_cmd_iperf,
        .argtable = &iperf_args
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&iperf_cmd) );
}
