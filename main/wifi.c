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

#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_coexist.h"
#include "mdns.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "main.h"
#include "nvs.h"
#include "wifi.h"
#include "cmd.h"
#include "time.h"
#include "wss.h"

#define   WIFI_MAX_SSID_LENGTH    32
#define   WIFI_MAX_PASSWD_LENGTH  32

#define FLAG_CONNECTED            BIT0
#define FLAG_DISCONNECT           BIT1
#define FLAG_GOT_IP_TCP           BIT2
#define FLAG_GOT_IP_UDP           BIT3
#define FLAG_GOT_IP_UDP_TIME_SYNC BIT4

static struct {
    int udpSocket;
    int tcpSocket;
    struct sockaddr_in udp_addr;
    struct sockaddr_in udp_time_sync_addr;

    bool reconnect;
    esp_netif_t *netif_ap;
    esp_netif_t *netif_sta;
    EventGroupHandle_t event_group;

    struct {
        nvs_handle_t nvsHandle;
        char         currentSsid[32];
        char         currentPasswd[32];
        uint8_t      recvBuf[1024];
    } wifi;

} g_server = {
    .reconnect = true,
};

static bool _mdnsInit(void)
{
    INFO("mDNS init\n");
    esp_err_t err = mdns_init();
    if (err) {
        ERROR("MDNS Init failed: %d\n", err);
        return false;
    }

    //set hostname
    mdns_hostname_set("pnu-esp32");
    //set default instance
    mdns_instance_name_set("Jhon's ESP32 Thing");

    return true;
}

static bool _nvs_get_ssid(char* ssid, char* passwd)
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

static bool _nvs_set_ssid(char* ssid, char* passwd)
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
    xEventGroupClearBits(g_server.event_group, FLAG_DISCONNECT);
    xEventGroupSetBits(g_server.event_group, FLAG_CONNECTED);
    xEventGroupSetBits(g_server.event_group, FLAG_GOT_IP_TCP);
    xEventGroupSetBits(g_server.event_group, FLAG_GOT_IP_UDP);
    xEventGroupSetBits(g_server.event_group, FLAG_GOT_IP_UDP_TIME_SYNC);

    _nvs_set_ssid(g_server.wifi.currentSsid, g_server.wifi.currentPasswd);
    _mdnsInit();
}

static void disconnect_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (g_server.reconnect) {
        TRACE("sta disconnect, reconnect...\n");
        esp_wifi_connect();
    } else {
        INFO("sta disconnect\n");
    }
    xEventGroupClearBits(g_server.event_group, FLAG_CONNECTED);
    xEventGroupSetBits(g_server.event_group, FLAG_DISCONNECT);

    xEventGroupClearBits(g_server.event_group, FLAG_GOT_IP_TCP);
    xEventGroupClearBits(g_server.event_group, FLAG_GOT_IP_UDP);
    xEventGroupClearBits(g_server.event_group, FLAG_GOT_IP_UDP_TIME_SYNC);
}

static void _udp_time_server(void)
{
    //esp_netif_ip_info_t ip;
    struct sockaddr_in listen_addr4 = { 0 };
    //struct sockaddr_storage listen_addr = { 0 };
    int actual_recv = 0;
    uint8_t buf[64];
    socklen_t socklen = sizeof(struct sockaddr_in);
    int s;

    INFO("_udp_time_server\n");

    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    bind(s, (struct sockaddr *)&listen_addr4, sizeof(listen_addr4));

    while (true) {
        actual_recv = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *)&g_server.udp_time_sync_addr, &socklen);

        if (actual_recv < 0) {
            WARN("udp time sync recvfrom error, error code: %d\n", actual_recv);
            break;
        } else {
            int64_t time;
            int64_t lastUpdated;
            int     txs;

            //CMD_CONTEXT	cmdContext = {
            //    .p_cbSend	= _sendUdpTo,
            //};
            
            txs = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (!txs) {
                ERROR("socket error\n");
            }

            //cmdContext.socket = txs;

            TIME_get64(&time);
            TIME_getUpdateTime(&lastUpdated);

            buf[actual_recv] = 0;
           	int64_t t = strtoull((char*)buf, NULL, 10);
            int64_t dt = time - t;
            TIME_set64(t);

            TRACE("udp from: %08x1n", g_server.udp_time_sync_addr.sin_addr);
            TRACE_BUF("udp recv", PRINT_BUF_STYLE_ASC_SIZE_NL, buf, actual_recv);
            INFO("dt: " PRINT_FRAC_STR(3) " since:%dms\n",
                PRINT_FRAC_ARGS(dt, 1000, 1000),
                (uint32_t)((time - lastUpdated)/1000));

            //CMD_sendTimeSyncAck(&cmdContext, dt);
            close(txs);
        }
    }

    if (s!= -1) {
        INFO("client socket closed.\n");
        close(s);
    }
    INFO("_udp_time_server exited\n");
}

static void task_tcp_server(void *arg)
{
    esp_ip4_addr_t  ip  = {0};
    httpd_handle_t  wssHandle = NULL;

    INFO("TCP started\n");

    while(true) {
        if (!ip.addr) {
            TRACE("waiting for FLAG_GOT_IP\n");
            int bits = xEventGroupWaitBits(g_server.event_group, FLAG_GOT_IP_TCP, 1, 1, 1000);

            if (bits & FLAG_GOT_IP_TCP) {
                INFO("got FLAG_GOT_IP\n");
                ip = wifi_getSelfIp();
                INFO("got ip=%08x\n", ip.addr);
            }
        }
    
        if (!ip.addr) {
            continue;
        }

        if (!wssHandle) {
            wssHandle = wss_start_server();

            if (wssHandle) {
                INFO("WSS server started!\n");
            }

        }
        vTaskDelay(1000);
        ip = wifi_getSelfIp();
    }
}

static void task_udp_time_server(void *arg)
{
    esp_ip4_addr_t  ip  = {0};

    INFO("UDP server started\n");

    while(true) {
        if (!ip.addr) {
            TRACE("UDP waiting for FLAG_GOT_IP\n");
            int bits = xEventGroupWaitBits(g_server.event_group, FLAG_GOT_IP_UDP_TIME_SYNC, 1, 1, 1000);

            if (bits & FLAG_GOT_IP_UDP_TIME_SYNC) {
                ip = wifi_getSelfIp();
                INFO("UDP server got ip=%08x\n", ip.addr);
            }
        }
    
        if (!ip.addr) {
            continue;
        }

        _udp_time_server();
        ip = wifi_getSelfIp();
    }

    vTaskDelete(NULL);
}

static int _startServer(void)
{
    BaseType_t ret;

    INFO("starting listener tasks\n");

    ret = xTaskCreate(task_tcp_server, "tcp_server", 8192, NULL, 4, NULL);
    if (ret != pdPASS) {
        ERROR("create task %s failed\n", task_tcp_server);
        return ESP_FAIL;
    }

    ret = xTaskCreate(task_udp_time_server, "udp_time", 8192, NULL, 3, NULL);
    if (ret != pdPASS) {
        ERROR("create task %s failed\n", task_udp_time_server);
        return ESP_FAIL;
    }

    return ESP_OK;
}

bool    SER_sendUdp(void* i_pBuf, uint16_t len)
{
    int sent;

    //TRACE_BUF("UDP tx", PRINT_BUF_STYLE_HEX_SIZE_NL, i_pBuf, len);
    sent = sendto(g_server.udpSocket, i_pBuf, len, 0, (struct sockaddr*)&g_server.udp_addr, sizeof(g_server.udp_addr));

    if (len != sent) {
        TRACE("send %d\n", sent);
    }

    return true;
}


bool    SER_sendTcp(void* i_pBuf, uint16_t len)
{
    int sent;

    TRACE_BUF("TCP tx", PRINT_BUF_STYLE_HEX_SIZE_NL, i_pBuf, len);
    sent = send(g_server.tcpSocket, i_pBuf, len, 0);

    if (len != sent) {
        ERROR("send s:%d len:%d sent:%d\n", g_server.tcpSocket, len, sent);
    }

    return true;
}

void initialise_wifi(void)
{
    static bool initialized = false;

    if (initialized) {
        return;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    g_server.event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_create_default() );
    g_server.netif_ap = esp_netif_create_default_wifi_ap();
    assert(g_server.netif_ap);
    g_server.netif_sta = esp_netif_create_default_wifi_sta();
    assert(g_server.netif_sta);
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

        ret = _nvs_get_ssid(ssid, passwd);
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
    strcpy(g_server.wifi.currentSsid, ssid);
    strcpy(g_server.wifi.currentPasswd, pass);

    int bits = xEventGroupWaitBits(g_server.event_group, FLAG_CONNECTED, 0, 1, 0);

    wifi_config_t wifi_config = { 0 };

    strlcpy((char *) wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    if (pass) {
        strlcpy((char *) wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
    }

    if (bits & FLAG_CONNECTED) {
        g_server.reconnect = false;
        xEventGroupClearBits(g_server.event_group, FLAG_CONNECTED);
        ESP_ERROR_CHECK( esp_wifi_disconnect() );
        xEventGroupWaitBits(g_server.event_group, FLAG_DISCONNECT, 0, 1, portTICK_PERIOD_MS);
    }

    g_server.reconnect = true;
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    esp_wifi_connect();

    xEventGroupWaitBits(g_server.event_group, FLAG_DISCONNECT, 0, 1, 5000 / portTICK_PERIOD_MS);

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

    esp_netif_get_ip_info(g_server.netif_sta, &ip);

    return ip.ip;
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

static bool dbgWssSend(uint8_t argc, char **argv)
{
    size_t  len;
    if (argc < 2) {
        return false;
    }

    len = strlen(argv[1]);

    wss_send(NULL, argv[1], len);

    return true;
}

static bool dbgStatus(uint8_t argc, char **argv)
{
    wifi_config_t cfg;
    wifi_mode_t mode;
    esp_netif_ip_info_t ip;

    esp_wifi_get_mode(&mode);
    if (WIFI_MODE_AP == mode) {
        esp_wifi_get_config(WIFI_IF_AP, &cfg);
        INFO("AP mode, %s %s\n", cfg.ap.ssid, cfg.ap.password);
    } else if (WIFI_MODE_STA == mode) {
        int bits = xEventGroupWaitBits(g_server.event_group, FLAG_CONNECTED, 0, 1, 0);
        if (bits & FLAG_CONNECTED) {
            esp_wifi_get_config(WIFI_IF_STA, &cfg);
            INFO("sta mode, connected %s\n", cfg.ap.ssid);
        } else {
            INFO("sta mode, disconnected\n");
        }
    } else {
        INFO("NULL mode %d\n", mode);
        return true;
    }

    memset(&ip, 0, sizeof(esp_netif_ip_info_t));

    if (esp_netif_get_ip_info(g_server.netif_sta, &ip) == 0) {
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
            nvs_close(g_server.wifi.nvsHandle);
            g_server.wifi.nvsHandle = (nvs_handle_t)NULL;
        } else if (!strcmp(argv[1], "commit")) {
            printf("commit\n");
            err = nvs_commit(g_server.wifi.nvsHandle);
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
            ret = _nvs_get_ssid(ssid, passwd);
            if (ret) {
                printf("%s:%s\n\n", ssid, passwd);
            }
        }
    } else if (argc == 3) {
        if (!strcmp(argv[1], "open\n")) {
            printf("open %s\n", argv[2]);
            err = nvs_open(argv[2], NVS_READWRITE, &g_server.wifi.nvsHandle);
        } else if (!strcmp(argv[1], "get\n")) {
            char    str[256];
             size_t length;

            printf("get %s\n", argv[2]);
            err =  nvs_get_str(g_server.wifi.nvsHandle, argv[2], str, &length);
            if (err == ESP_OK) {
                str[length] = '\0';
                printf("str=<%s>\n", str);
            }
        }
    } else if (argc == 4) {
        if (!strcmp(argv[1], "set")) {
            printf("set %s <- %s\n", argv[2], argv[3]);
            err = nvs_set_str(g_server.wifi.nvsHandle, argv[2], argv[3]);
        } else if (!strcmp(argv[1], "ssid")) {
            _nvs_set_ssid(argv[2], argv[3]);
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

static bool dbgBroadcastTime(uint8_t argc, char **argv)
{
    int s;
    struct sockaddr_in dest = { 0 };
    int opt = 1;
    int64_t time;

    char buf[32];
    int     len;
    int     sent;

    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (!s) {
        ERROR("socket error\n");
    }
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));

    dest.sin_family = AF_INET;
    inet_aton("192.168.1.255", &dest.sin_addr);
    dest.sin_port = htons(UDP_TIME_SERVER_PORT);

    TIME_get64(&time);
    len = sprintf(buf, "%d%d", (uint32_t)(time/1000000), (uint32_t)(time%1000000));

    sent = sendto(s, buf, len, 0, (struct sockaddr*)&dest, sizeof(dest));
    if (sent != len) {
        ERROR("sendto %d != %d", sent, len);
    }
    close(s);

    return true;
}

static bool dbgMdns(uint8_t argc, char **argv)
{
    _mdnsInit();

    return true;
}

// *INDENT-OFF*
DEBUG_MENU_START(g_menu)
	DEBUG_MENU_DIR("wifi", NULL)
		DEBUG_MENU_CMD("status",	        NULL,		NULL, dbgStatus)
		DEBUG_MENU_CMD("apn",	            NULL,		NULL, dbgConnect)
		DEBUG_MENU_CMD("scan",	            NULL,		NULL, dbgScan)
		DEBUG_MENU_CMD("wssSend",           NULL,		NULL, dbgWssSend)
		DEBUG_MENU_CMD("nvs",	            NULL,		NULL, dbgNvs)
		DEBUG_MENU_CMD("broadcastUdpTime",	NULL,		NULL, dbgBroadcastTime)
		DEBUG_MENU_CMD("mdns",          	NULL,		NULL, dbgMdns)
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*

void register_wifi(void)
{
	DBG_TREE_add("/", g_menu);
}
