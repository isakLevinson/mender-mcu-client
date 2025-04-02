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
#include "parseArgs.h"

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_coexist.h"
#include "mdns.h"
#include "esp_mac.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "main.h"
#include "nvs.h"
#include "wifi.h"
#include "cmd.h"
#include "time.h"
#include "wss.h"
#include "factory.h"
#include "config.h"
#include "mender_ota.h"

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
	esp_netif_t* netif_ap;
	esp_netif_t* netif_sta;
	EventGroupHandle_t event_group;

	struct {
		char         currentSsid[32];
		char         currentPasswd[32];
		uint8_t      recvBuf[1024];
	} wifi;

} g_server = {
	.reconnect = true,
};

static bool _mdnsInit(void)
{
	bool    ret;
	char*	sn;

	INFO("mDNS init\n");
	esp_err_t err = mdns_init();
	if (err) {
		ERROR("MDNS Init failed: %d\n", err);
		return false;
	}

	ret = FACTORY_factoryGetSn(&sn);
	if (ret) {
		INFO("setting MDNS to SN %s\n", sn);
		mdns_hostname_set(sn);
	} else {
		ERROR("MDNS not defined, and no SN in configuration\n");
	}

	//set default instance
	mdns_instance_name_set("Jhon's ESP32 Thing");

	return true;
}

static void scan_done_handler(void* arg, esp_event_base_t event_base,
    int32_t event_id, void* event_data)
{
	uint16_t sta_number = 0;
	uint8_t i;
	wifi_ap_record_t* ap_list_buffer;

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

	if (esp_wifi_scan_get_ap_records(&sta_number, (wifi_ap_record_t*)ap_list_buffer) == ESP_OK) {
		for (i = 0; i < sta_number; i++) {
			INFO("[%s][rssi=%d]\n", ap_list_buffer[i].ssid, ap_list_buffer[i].rssi);
		}
	}
	free(ap_list_buffer);
	INFO("sta scan done\n");
}

static void got_ip_handler(void* arg, esp_event_base_t event_base,
    int32_t event_id, void* event_data)
{
	INFO("got_ip_handler\n");
	xEventGroupClearBits(g_server.event_group, FLAG_DISCONNECT);
	xEventGroupSetBits(g_server.event_group, FLAG_CONNECTED);
	xEventGroupSetBits(g_server.event_group, FLAG_GOT_IP_TCP);
	xEventGroupSetBits(g_server.event_group, FLAG_GOT_IP_UDP);
	xEventGroupSetBits(g_server.event_group, FLAG_GOT_IP_UDP_TIME_SYNC);

	NVS_set_ssid(g_server.wifi.currentSsid, g_server.wifi.currentPasswd);
	WIFI_stopAp();
	wss_config_stop();
	MENDER_execute();
}

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
	char* strEvent  = NULL;
	char* strBase    = "";

	if (event_base == WIFI_EVENT) {
		strBase = "WIFI";
		switch (event_id) {
			case WIFI_EVENT_AP_STACONNECTED:
				strEvent = "WIFI_EVENT_AP_STACONNECTED";
				break;
			case WIFI_EVENT_AP_STADISCONNECTED:
				strEvent = "WIFI_EVENT_AP_STADISCONNECTED";
				break;
			case WIFI_EVENT_STA_START:
				strEvent = "WIFI_EVENT_STA_START";
				break;
			case WIFI_EVENT_AP_START:
				strEvent = "WIFI_EVENT_AP_START";
				break;
			case WIFI_EVENT_HOME_CHANNEL_CHANGE:
				strEvent = "WIFI_EVENT_HOME_CHANNEL_CHANGE";
				break;
			case WIFI_EVENT_STA_CONNECTED:
				strEvent = "WIFI_EVENT_STA_CONNECTED";
				break;



			default:
		}
	} else if (event_base == IP_EVENT) {
		strBase = "IP";
		switch (event_id) {
			case IP_EVENT_AP_STAIPASSIGNED:
				strEvent = "IP_EVENT_AP_STAIPASSIGNED";
				break;
			default:
		}
	} else {
		strBase = "";
	}

	if (strEvent) {
		INFO("event_handler %s\n", strEvent);
	} else {
		INFO("event_handler %s %d\n", strBase, event_id);
	}
}

static void disconnect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
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
	bind(s, (struct sockaddr*)&listen_addr4, sizeof(listen_addr4));

	while (true) {
		actual_recv = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr*)&g_server.udp_time_sync_addr, &socklen);

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
			    (uint32_t)((time - lastUpdated) / 1000));

			//CMD_sendTimeSyncAck(&cmdContext, dt);
			close(txs);
		}
	}

	if (s != -1) {
		INFO("client socket closed.\n");
		close(s);
	}
	INFO("_udp_time_server exited\n");
}

static esp_ip4_addr_t  wifi_getSelfIp(void)
{
	esp_netif_ip_info_t ip;

	memset(&ip, 0, sizeof(esp_netif_ip_info_t));

	esp_netif_get_ip_info(g_server.netif_sta, &ip);

	return ip.ip;
}

static void task_tcp_server(void* arg)
{
	bool	ret;
	esp_ip4_addr_t  ip  = {0};

	INFO("TCP started\n");

	while (true) {
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

		vTaskDelay(1000);
		ip = wifi_getSelfIp();
	}
}

static void task_udp_time_server(void* arg)
{
	esp_ip4_addr_t  ip  = {0};

	INFO("UDP server started\n");

	while (true) {
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

	wss_start_server();

	ret = xTaskCreate(task_tcp_server, "tcp_server", 8192, NULL, 4, NULL);
	if (ret != pdPASS) {
		ERROR("create task %s failed\n", task_tcp_server);
		return ESP_FAIL;
	}

	ret = xTaskCreate(task_udp_time_server, "udp_time", 4096, NULL, 3, NULL);
	if (ret != pdPASS) {
		ERROR("create task %s failed\n", task_udp_time_server);
		return ESP_FAIL;
	}

	return ESP_OK;
}

static bool _startSta(void)
{
	esp_err_t err;

	wifi_config_t wifi_sta_config = {
		.sta = {
			.scan_method = WIFI_ALL_CHANNEL_SCAN,
			.failure_retry_cnt = 5,
			/* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (password len => 8).
			 * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
			 * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
			* WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
			 */
			.threshold.authmode = WIFI_AUTH_WPA2_PSK,//ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
			.sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
		},
	};

	g_server.netif_sta = esp_netif_create_default_wifi_sta();
	assert(g_server.netif_sta);

	err = esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config);
	if (ESP_OK != err) {
		ERROR("esp_wifi_set_config WIFI_IF_STA %d 0x%x\n", err, err);
		return false;
	}

	return true;
}

static bool _startAp(void)
{
	bool	ret;
	esp_err_t err;
	char*	sn;

	wifi_config_t wifi_ap_config = {
		.ap = {
			//.ssid = "",
			.ssid_len = 3,
			.password = "",
			.channel = 5,
			.max_connection = 1,
			.authmode = WIFI_AUTH_OPEN,
			.pmf_cfg = {
				.required = false,
			},
		},
	};

	g_server.netif_ap  = esp_netif_create_default_wifi_ap();
	assert(g_server.netif_ap);

	ret = FACTORY_factoryGetSn(&sn);
	if (ret) {
		sprintf((char*)wifi_ap_config.ap.ssid, "%s", sn);
		wifi_ap_config.ap.ssid_len = strlen((char*)wifi_ap_config.ap.ssid);
		INFO_BUF("ssid", PRINT_BUF_STYLE_ASC_SIZE_NL, wifi_ap_config.ap.ssid, wifi_ap_config.ap.ssid_len);
	}

	err = esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config);
	if (ESP_OK != err) {
		ERROR("esp_wifi_set_config WIFI_IF_AP %d 0x%x\n", err, err);
		return false;
	}

	return true;
}

static void _init(void)
{
	static bool initialized = false;
	esp_err_t err;
	bool    ret = true;
	char    ssid[32];
	char    passwd[32];


	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

	if (initialized) {
		return;
	}

	ESP_ERROR_CHECK(esp_netif_init());
	g_server.event_group = xEventGroupCreate();
	ESP_ERROR_CHECK(esp_event_loop_create_default());

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

	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
	        ESP_EVENT_ANY_ID,
	        &event_handler,
	        NULL,
	        NULL));

	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	_startAp();
	_startSta();

	ESP_ERROR_CHECK(esp_wifi_start());
	esp_netif_set_default_netif(g_server.netif_sta);
	_startServer();

	ret = NVS_get_ssid(ssid, passwd);
	if (ret) {
		INFO("ssid  : %s\n", ssid);
		INFO("passwd: %s\n", passwd);
		WIFI_stopAp();
		WIFI_sta_connect(ssid, passwd);
	} else {
		INFO("WSS and PASSWD not set\n");
		ret = FACTORY_factoryGetSn(NULL);
		if (ret) {
			INFO("Starting AP and configuration server\n");
			// start AP and configuration server only if SN has not been set yet
			WIFI_startAp();
			wss_config_start();
		} else {
			WARN("SN not set. will not start configuration mode\n");
		}
	}

	_mdnsInit();

	initialized = true;
}

static bool _sta_scan(const char* ssid)
{
	wifi_scan_config_t scan_config = { 0 };
	scan_config.ssid = (uint8_t*) ssid;

	esp_wifi_scan_start(&scan_config, false);

	return true;
}

bool WIFI_sta_disconnect(void)
{
	g_server.reconnect = false;
	xEventGroupClearBits(g_server.event_group, FLAG_CONNECTED);
	ESP_ERROR_CHECK(esp_wifi_disconnect());
	xEventGroupWaitBits(g_server.event_group, FLAG_DISCONNECT, 0, 1, portTICK_PERIOD_MS);

	return true;
}

bool WIFI_sta_connect(const char* ssid, const char* pass)
{
	strcpy(g_server.wifi.currentSsid, ssid);
	strcpy(g_server.wifi.currentPasswd, pass);

	int bits = xEventGroupWaitBits(g_server.event_group, FLAG_CONNECTED, 0, 1, 0);

	wifi_config_t wifi_config = { 0 };

	strlcpy((char*) wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
	if (pass) {
		strlcpy((char*) wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
	}

	if (bits & FLAG_CONNECTED) {
		WIFI_sta_disconnect();
	}

	g_server.reconnect = true;
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
	esp_wifi_connect();

	xEventGroupWaitBits(g_server.event_group, FLAG_DISCONNECT, 0, 1, 5000 / portTICK_PERIOD_MS);

	return true;
}

bool	WIFI_isConnected(void)
{
	int bits = xEventGroupWaitBits(g_server.event_group, FLAG_CONNECTED, 0, 1, 0);
	if (bits & FLAG_CONNECTED) {
		return true;
	} else {
		return false;
	}
}

bool WIFI_startAp(void)
{
	esp_wifi_set_mode(WIFI_MODE_APSTA);
	return true;
}

bool WIFI_stopAp(void)
{
	esp_wifi_set_mode(WIFI_MODE_STA);
	return true;
}

static bool dbgConnect(uint8_t argc, char** argv)
{
	if (argc < 3) {
		return false;
	}

	WIFI_sta_connect(argv[1], argv[2]);

	return true;
}

static bool dbgScan(uint8_t argc, char** argv)
{
	_sta_scan(NULL);
	return true;
}

static bool dbgWssSend(uint8_t argc, char** argv)
{
	size_t  len;
	if (argc < 2) {
		return false;
	}

	len = strlen(argv[1]);

	wss_send(NULL, argv[1], len);

	return true;
}

static bool dbgStatus(uint8_t argc, char** argv)
{
	int				err;
	wifi_config_t	cfg;
	wifi_mode_t		mode;
	esp_netif_ip_info_t ip;

	esp_wifi_get_mode(&mode);

	bool    useSTA = ((WIFI_MODE_APSTA == mode) || ((WIFI_MODE_STA == mode)));
	bool    useAP = ((WIFI_MODE_APSTA == mode) || ((WIFI_MODE_AP == mode)));

	uint8_t mac[6];
	err = esp_efuse_mac_get_default(mac);
	if (err) {
		ERROR("esp_efuse_mac_get_default failed %d\n", err);
	} else {
		PRINT("default MAC %02x%02x%02x%02x%02x%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	}

	if (useAP) {
		esp_wifi_get_config(WIFI_IF_AP, &cfg);
		INFO("AP mode, %s %s\n", cfg.ap.ssid, cfg.ap.password);
	}

	if (useSTA) {
		bool isConnected = WIFI_isConnected();
		if (isConnected) {
			esp_wifi_get_config(WIFI_IF_STA, &cfg);
			INFO("sta mode, connected %s\n", cfg.ap.ssid);
		} else {
			INFO("sta mode, disconnected\n");
		}
	}

	memset(&ip, 0, sizeof(esp_netif_ip_info_t));

	if (esp_netif_get_ip_info(g_server.netif_sta, &ip) == 0) {
		INFO("STA:" IPSTR " " IPSTR " " IPSTR"\n", IP2STR(&ip.ip), IP2STR(&ip.netmask), IP2STR(&ip.gw));
	}

	if (esp_netif_get_ip_info(g_server.netif_ap, &ip) == 0) {
		INFO("AP:" IPSTR " " IPSTR " " IPSTR"\n", IP2STR(&ip.ip), IP2STR(&ip.netmask), IP2STR(&ip.gw));
	}

	return true;
}

static bool dbgBroadcastTime(uint8_t argc, char** argv)
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
	len = sprintf(buf, "%d%d", (uint32_t)(time / 1000000), (uint32_t)(time % 1000000));

	sent = sendto(s, buf, len, 0, (struct sockaddr*)&dest, sizeof(dest));
	if (sent != len) {
		ERROR("sendto %d != %d", sent, len);
	}
	close(s);

	return true;
}

static bool dbgAp(uint8_t argc, char** argv)
{
	if (argc < 2) {
		return false;
	}

	if ('0' == argv[1][0]) {
		WIFI_stopAp();
	}

	if ('1' == argv[1][0]) {
		WIFI_startAp();
	}

	return true;
}

static bool dbgConfig(uint8_t argc, char** argv)
{
	if (argc < 2) {
		return false;
	}

	if ('0' == argv[1][0]) {
		wss_config_stop();
	}

	if ('1' == argv[1][0]) {
		wss_config_start();
	}

	return true;
}

// *INDENT-OFF*
DEBUG_MENU_START(g_menu)
	DEBUG_MENU_DIR("wifi", NULL)
		DEBUG_MENU_CMD("status",	        NULL,		NULL, dbgStatus)
		DEBUG_MENU_CMD("apn",	            NULL,		NULL, dbgConnect)
		DEBUG_MENU_CMD("scan",	            NULL,		NULL, dbgScan)
		DEBUG_MENU_CMD("wssSend",           NULL,		NULL, dbgWssSend)
		DEBUG_MENU_CMD("broadcastUdpTime",	NULL,		NULL, dbgBroadcastTime)
		DEBUG_MENU_CMD("ap",	          	"<0/1>",    NULL, dbgAp)
		DEBUG_MENU_CMD("config",          	"<0/1>",    NULL, dbgConfig)
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*

void WIFI_init(void)
{
	DBG_TREE_add("/", g_menu);

	_init();
}
