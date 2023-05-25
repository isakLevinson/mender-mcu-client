/* Iperf Example - wifi commands

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#define DEF_DBG_MODULE	DBG_MODULE_WIFI
#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include "iperf.h"

#include "esp_log.h"
#include "esp_console.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "esp_coexist.h"

#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "driver/uart.h"
#include "main.h"
#include "cmd_wifi.h"
#include "nvs.h"
#include "wifi.h"

typedef bool (*CMD_RESPONSE_CB)(void* pArg, void* i_pBuf);

#define   WIFI_MAX_SSID_LENGTH    32
#define   WIFI_MAX_PASSWD_LENGTH  32

typedef struct {
    int    Socket;
    int    ListenSocket;
    int     uart;
} CHANNEL;

CHANNEL g_channel;

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

static struct {
    nvs_handle_t nvsHandle;
    char    currentSsid[32];
    char    currentPasswd[32];
    uint8_t recvBuf[1024];
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

int socket_cmd = -1;
int socket_listen_cmd = -1;
int socket_stream = -1;
int socket_listen_stream = -1;

static bool reconnect = true;
static const char *TAG = "cmd_wifi";
static esp_netif_t *netif_ap = NULL;
static esp_netif_t *netif_sta = NULL;

static EventGroupHandle_t wifi_event_group;
const int FLAG_CONNECTED  = BIT0;
const int FLAG_DISCONNECT = BIT1;
const int FLAG_GOT_IP     = BIT2;

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
        printf("nvs_open <%s> failed %x\n\n",  NVS_NAMESPACE_WIFI, err);
        return false;
    }

    length = WIFI_MAX_SSID_LENGTH;
    err =  nvs_get_str(handle, NVS_KEY_WIFI_SSID, ssid, &length);
    if (err != ESP_OK) {
        printf("nvs_get_str ssid failed\n\n");
        ret = false;
        goto exit;
    }
    ssid[length] = '\0';

    length = WIFI_MAX_PASSWD_LENGTH;
    err =  nvs_get_str(handle, NVS_KEY_WIFI_PASSWD, passwd, &length);
    if (err != ESP_OK) {
        printf("nvs_get_str passwd failed\n\n");
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
        printf("%s\n\n", pStr);
    } else {
        if (ESP_OK != err) {
            printf("0x%x\n", err);
        }
    }

    return ret;
}

bool    wifi_nvs_set_ssid(char* ssid, char* passwd)
{
    bool    ret = true;
    esp_err_t err = ESP_OK;
    nvs_handle_t handle;
    char    str[WIFI_MAX_SSID_LENGTH];
    size_t  length;

    err = nvs_open(NVS_NAMESPACE_WIFI, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ERROR("nvs_open failed\n");
        return false;
    }

    length = WIFI_MAX_SSID_LENGTH;
    err =  nvs_get_str(handle, NVS_KEY_WIFI_SSID, str, &length);
    if (err != ESP_OK) {
        WARN("nvs_get_str ssid failed\n");
        goto    store;
    }
    str[length] = '\0';
    if (strcmp(str, ssid)) {
        INFO("ssid mismatch. storing new <%s> <%s>\n", ssid, passwd);
        goto store;
    }
    length = WIFI_MAX_PASSWD_LENGTH;
    err =  nvs_get_str(handle, NVS_KEY_WIFI_PASSWD, str, &length);
    if (err != ESP_OK) {
        WARN("nvs_get_str passwd failed\n");
        goto    store;
    }
    str[length] = '\0';
    if (strcmp(str, passwd)) {
        INFO("passwd mismatch. storing new <%s> <%s>\n", ssid, passwd);
        goto store;
    }

    INFO("no need to store ssid or passwd\n");
    goto exit;

    store:
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
        ERROR("No AP found\n");
        return;
    }

    ap_list_buffer = malloc(sta_number * sizeof(wifi_ap_record_t));
    if (ap_list_buffer == NULL) {
        ERROR("Failed to malloc buffer to print scan results\n");
        return;
    }

    if (esp_wifi_scan_get_ap_records(&sta_number, (wifi_ap_record_t *)ap_list_buffer) == ESP_OK) {
        for (i = 0; i < sta_number; i++) {
            INFO("[%s][rssi=%d]\n", ap_list_buffer[i].ssid, ap_list_buffer[i].rssi);
        }
    }
    free(ap_list_buffer);
    INFO("sta scan done\n");
}

static void got_ip_handler(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    INFO("got_ip_handler\n");
    xEventGroupClearBits(wifi_event_group, FLAG_DISCONNECT);
    xEventGroupSetBits(wifi_event_group, FLAG_CONNECTED);
    xEventGroupSetBits(wifi_event_group, FLAG_GOT_IP);

    wifi_nvs_set_ssid(g_wifi.currentSsid, g_wifi.currentPasswd);
}

static void _socket_close(int* pSocket)
{
    shutdown(*pSocket, 0);
    close(*pSocket);
    *pSocket = -1;
}

static void disconnect_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (reconnect) {
        INFO("sta disconnect, reconnect...\n");
        esp_wifi_connect();
    } else {
        INFO("sta disconnect\n");
    }
    xEventGroupClearBits(wifi_event_group, FLAG_CONNECTED);
    xEventGroupSetBits(wifi_event_group, FLAG_DISCONNECT);

    xEventGroupClearBits(wifi_event_group, FLAG_GOT_IP);

    _socket_close(&socket_listen_cmd);
    _socket_close(&socket_listen_stream);
}

static void cmd_tcp_server(void)
{
    esp_err_t ret = ESP_OK;
    int err = 0;
    CHANNEL* pChannel = &g_channel;

    esp_netif_ip_info_t ip;
    struct sockaddr_in listen_addr4 = { 0 };
    struct sockaddr_storage listen_addr = { 0 };
    struct sockaddr_in remote_addr;
    //struct timeval timeout = { 0 };
    socklen_t addr_len = sizeof(struct sockaddr);
    int opt = 1;

    INFO("listener loop started\n");

    if (esp_netif_get_ip_info(netif_sta, &ip) == 0) {
        INFO("IP:" IPSTR "\n", IP2STR(&ip.ip));
        INFO("MASK:" IPSTR "\n", IP2STR(&ip.netmask));
        INFO("GW:" IPSTR "\n", IP2STR(&ip.gw));

        listen_addr4.sin_addr.s_addr = ip.ip.addr;

    } else {
        ERROR("esp_netif_get_ip_info failed\n");
        return;
    }

    listen_addr4.sin_family = AF_INET;
    listen_addr4.sin_port = htons(TCP_CMD_PORT);

    pChannel->ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ESP_GOTO_ON_FALSE((pChannel->ListenSocket >= 0), ESP_FAIL, exit, TAG, "Unable to create socket: errno %d\n", errno);

    setsockopt(pChannel->ListenSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    INFO("listen socket created\n");

    err = bind(pChannel->ListenSocket, (struct sockaddr *)&listen_addr4, sizeof(listen_addr4));
    ESP_GOTO_ON_FALSE((err == 0), ESP_FAIL, exit, TAG, "Socket unable to bind: errno %d, IPPROTO: %d\n", errno, AF_INET);

    INFO("#4\n");

    err = listen(pChannel->ListenSocket, 5);
    ESP_GOTO_ON_FALSE((err == 0), ESP_FAIL, exit, TAG, "Error occurred during listen: errno %d\n", errno);
    memcpy(&listen_addr, &listen_addr4, sizeof(listen_addr4));

 //   INFO("listen on:"IPSTR, IP2STR(&listen_addr4));

    INFO("listen on addr %d.%d.%d.%d\n",
             listen_addr4.sin_addr.s_addr & 0xFF,
             (listen_addr4.sin_addr.s_addr >> 8) & 0xFF,
             (listen_addr4.sin_addr.s_addr >> 16) & 0xFF,
             (listen_addr4.sin_addr.s_addr >> 24) & 0xFF);

    pChannel->Socket = accept(pChannel->ListenSocket, (struct sockaddr *)&remote_addr, &addr_len);
    ESP_GOTO_ON_FALSE((pChannel->Socket >= 0), ESP_FAIL, exit, TAG, "Unable to accept connection: errno %d\n", errno);
    INFO("accept %s,%d\n\n", inet_ntoa(remote_addr.sin_addr), htons(remote_addr.sin_port));

    //timeout.tv_sec = IPERF_SOCKET_RX_TIMEOUT;
    //setsockopt(*pSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

/////////////////////////////////////////////////
    uint8_t *buffer;
    int want_recv = 0;
    int actual_recv = 0;
    socklen_t socklen = sizeof(struct sockaddr_in);

    buffer = g_wifi.recvBuf;
    want_recv = sizeof(g_wifi.recvBuf);
    while (true) {
        actual_recv = recvfrom(pChannel->Socket, buffer, want_recv, 0, (struct sockaddr *)&listen_addr, &socklen);
        if (actual_recv < 0) {
            //iperf_show_socket_error_reason(error_log, recv_socket);
            //WARN("error, error code: %d, reason: %s\n", error_log, strerror(error_log));
            WARN("recv error, error code: %d\n", actual_recv);

            //s_iperf_ctrl.finish = true;
            break;
        } else {
            INFO_BUF("recv",	PRINT_BUF_STYLE_HEX_SIZE_NL, buffer, actual_recv);
        }
    }

exit:
    if (pChannel->Socket != -1) {
        INFO("client socket closed.\n");
        //close(pChannel->pSocket);
        //pChannel->pSocket = -1;
        _socket_close(&pChannel->Socket);
    }

    if (pChannel->ListenSocket != -1) {
        _socket_close(&pChannel->ListenSocket);
        INFO("listener socket closed.\n");
    }

    if (ESP_OK != ret) {
        WARN("listener exit with ret=0x%x\n", ret);
    }
}

static void task_server(void *arg)
{
    esp_ip4_addr_t  ip  = {0};

    while(true) {
        if (!ip.addr) {
            INFO("waiting for FLAG_GOT_IP\n");
            int bits = xEventGroupWaitBits(wifi_event_group, FLAG_GOT_IP, 1, 1, 1000);

            if (bits & FLAG_GOT_IP) {
                INFO("got FLAG_GOT_IP\n");
                ip = wifi_getSelfIp();
                INFO("got ip=%08x\n", ip.addr);
            }
        }
    
        if (!ip.addr) {
            continue;
        }

        cmd_tcp_server();
        ip = wifi_getSelfIp();
    }

    vTaskDelete(NULL);
}

static int _startServer(void)
{
    BaseType_t ret;

    INFO("starting listener task\n");

    ret = xTaskCreate(task_server, IPERF_TRAFFIC_TASK_NAME, IPERF_TRAFFIC_TASK_STACK, NULL, IPERF_TRAFFIC_TASK_PRIORITY, NULL);
    if (ret != pdPASS) {
        ERROR("create task %s failed\n", IPERF_TRAFFIC_TASK_NAME);
        return ESP_FAIL;
    }

    return ESP_OK;
}

void initialise_wifi(void)
{
    esp_log_level_set("wifi\n", ESP_LOG_WARN);
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

    _startServer();

    {
        bool    ret = true;
        char    ssid[32];
        char    passwd[32];

        ret = wifi_nvs_get_ssid(ssid, passwd);
        if (ret) {
            INFO("ssid  : %s\n\n", ssid);
            INFO("passwd: %s\n\n", passwd);
            wifi_cmd_sta_join(ssid, passwd);
        }
    }

    initialized = true;
}

bool wifi_cmd_sta_join(const char *ssid, const char *pass)
{
    strcpy(g_wifi.currentSsid, ssid);
    strcpy(g_wifi.currentPasswd, pass);

    int bits = xEventGroupWaitBits(wifi_event_group, FLAG_CONNECTED, 0, 1, 0);

    wifi_config_t wifi_config = { 0 };

    strlcpy((char *) wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    if (pass) {
        strlcpy((char *) wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
    }

    if (bits & FLAG_CONNECTED) {
        reconnect = false;
        xEventGroupClearBits(wifi_event_group, FLAG_CONNECTED);
        ESP_ERROR_CHECK( esp_wifi_disconnect() );
        xEventGroupWaitBits(wifi_event_group, FLAG_DISCONNECT, 0, 1, portTICK_PERIOD_MS);
    }

    reconnect = true;
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    esp_wifi_connect();

    xEventGroupWaitBits(wifi_event_group, FLAG_DISCONNECT, 0, 1, 5000 / portTICK_PERIOD_MS);

    return true;
}

static bool wifi_cmd_sta_scan(const char *ssid)
{
    wifi_scan_config_t scan_config = { 0 };
    scan_config.ssid = (uint8_t *) ssid;

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    esp_wifi_scan_start(&scan_config, false);

    return true;
}

esp_ip4_addr_t  wifi_getSelfIp(void)
{
    esp_netif_ip_info_t ip;

    memset(&ip, 0, sizeof(esp_netif_ip_info_t));

    esp_netif_get_ip_info(netif_sta, &ip);

    return ip.ip;
}

static uint32_t wifi_get_local_ip(void)
{
    int bits;
    esp_netif_t *netif = netif_ap;
    esp_netif_ip_info_t ip_info;
    wifi_mode_t mode;

    esp_wifi_get_mode(&mode);
//    INFO("wifi_get_local_ip mode=%d\n", mode);
    if (WIFI_MODE_STA == mode) {
        bits = xEventGroupWaitBits(wifi_event_group, FLAG_CONNECTED, 0, 1, 100);
        if (bits & FLAG_CONNECTED) {
            INFO("FLAG_CONNECTED ip=%08x\n", ip_info.ip.addr);
            netif = netif_sta;
        } else {
            ERROR("sta has no IP\n");
            return 0;
        }
    }

    esp_netif_get_ip_info(netif, &ip_info);
    return ip_info.ip.addr;
}

static bool dbgConnect(uint8_t argc, char** argv)
{
    if (argc < 3) {
        return false;
    }

    wifi_cmd_sta_join(argv[1], argv[2]);

    return true;
}

static bool dbgScan(uint8_t argc, char **argv)
{
    //wifi_cmd_sta_scan(scan_args.ssid->sval[0]);
    wifi_cmd_sta_scan(NULL);
    return true;
}

static bool dbgQuery(uint8_t argc, char **argv)
{
    wifi_config_t cfg;
    wifi_mode_t mode;
    esp_netif_ip_info_t ip;

    esp_wifi_get_mode(&mode);
    if (WIFI_MODE_AP == mode) {
        esp_wifi_get_config(WIFI_IF_AP, &cfg);
        INFO("AP mode, %s %s\n", cfg.ap.ssid, cfg.ap.password);
    } else if (WIFI_MODE_STA == mode) {
        int bits = xEventGroupWaitBits(wifi_event_group, FLAG_CONNECTED, 0, 1, 0);
        if (bits & FLAG_CONNECTED) {
            esp_wifi_get_config(WIFI_IF_STA, &cfg);
            INFO("sta mode, connected %s\n", cfg.ap.ssid);
        } else {
            INFO("sta mode, disconnected\n");
        }
    } else {
        INFO("NULL mode\n");
        return 0;
    }

    memset(&ip, 0, sizeof(esp_netif_ip_info_t));

    if (esp_netif_get_ip_info(netif_sta, &ip) == 0) {
        INFO("IP:" IPSTR "\n", IP2STR(&ip.ip));
        INFO("MASK:" IPSTR "\n", IP2STR(&ip.netmask));
        INFO("GW:" IPSTR "\n", IP2STR(&ip.gw));
    }
    return true;
}

static bool dbgNvs(uint8_t argc, char **argv)
{
    esp_err_t err = ESP_OK;

    if (argc == 2) {
        if (!strcmp(argv[1], "close")) {
            printf("close\n");
            nvs_close(g_wifi.nvsHandle);
            g_wifi.nvsHandle = (nvs_handle_t)NULL;
        } else if (!strcmp(argv[1], "commit")) {
            printf("commit\n");
            err = nvs_commit(g_wifi.nvsHandle);
        } else if (!strcmp(argv[1], "stats")) {
            nvs_stats_t nvs_stats;

            printf("stats\n\n");
            err =  nvs_get_stats(NULL, &nvs_stats);
            if (err == ESP_OK) {
                printf("used_entries   : %d\n", nvs_stats.used_entries);
                printf("free_entries   : %d\n", nvs_stats.free_entries);
                printf("total_entries  : %d\n", nvs_stats.total_entries);
                printf("namespace_count: %d\n", nvs_stats.namespace_count);
            }
        } else if (!strcmp(argv[1], "list\n")) {
            nvs_iterator_t it;
            printf("list\n\n");
            
            err =  nvs_entry_find(NVS_DEFAULT_PART_NAME, NULL, NVS_TYPE_ANY, &it);
            while (err == ESP_OK) {
                nvs_entry_info_t info;
                nvs_entry_info(it, &info); // Can omit error check if parameters are guaranteed to be non-NULL
                printf("ns: '%s', key: '%s', type: '%x'\n", info.namespace_name, info.key, info.type);

                switch (info.type) {
                    case NVS_TYPE_U8:   printf("U8\n");  break;
                    case NVS_TYPE_I8:   printf("I8\n");  break;
                    case NVS_TYPE_U16:  printf("U16\n");  break;
                    case NVS_TYPE_I16:  printf("I16\n");  break;
                    case NVS_TYPE_U32:  printf("U32\n");  break;
                    case NVS_TYPE_I32:  printf("I32\n");  break;
                    case NVS_TYPE_U64:  printf("U64\n");  break;
                    case NVS_TYPE_I64:  printf("I64\n");  break;
                    case NVS_TYPE_STR:  printf("STR\n");  break;
                    case NVS_TYPE_BLOB: printf("BLOB\n");  break;
                    default:
                }
                printf("\n");

                err = nvs_entry_next(&it);
            }
        } else if (!strcmp(argv[1], "ssid")) {
            bool ret;
            char ssid[WIFI_MAX_SSID_LENGTH] = "";
            char passwd[WIFI_MAX_PASSWD_LENGTH] = "";
            ret = wifi_nvs_get_ssid(ssid, passwd);
            if (ret) {
                printf("%s:%s\n\n", ssid, passwd);
            }
        }
    } else if (argc == 3) {
        if (!strcmp(argv[1], "open\n")) {
            printf("open %s\n", argv[2]);
            err = nvs_open(argv[2], NVS_READWRITE, &g_wifi.nvsHandle);
        } else if (!strcmp(argv[1], "get\n")) {
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
            printf("failed %s\n\n", pStr);
        } else {
            printf("failed 0x%x\n\n", err);
        }
    }

    return true;
}

// *INDENT-OFF*
DEBUG_MENU_START(g_menu)
	DEBUG_MENU_DIR("wifi", NULL)
		DEBUG_MENU_CMD("apn",	NULL,		NULL, dbgConnect)
		DEBUG_MENU_CMD("scan",	NULL,		NULL, dbgScan)
		DEBUG_MENU_CMD("query",	NULL,		NULL, dbgQuery)
		DEBUG_MENU_CMD("nvs",	NULL,		NULL, dbgNvs)
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*


void register_wifi(void)
{
	DBG_TREE_add("/\n", g_menu);
}
