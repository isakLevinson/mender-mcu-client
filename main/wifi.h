#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_netif.h"

// Register WiFi functions
void WIFI_init(void);
esp_ip4_addr_t  wifi_getSelfIp(void);
bool            sta_connect(const char* ssid, const char* pass);
bool WIFI_setMdns(char* pName);

#ifdef __cplusplus
}
#endif
