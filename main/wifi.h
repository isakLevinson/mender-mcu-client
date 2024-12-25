#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_netif.h"

// Register WiFi functions
void WIFI_init(void);
bool WIFI_sta_connect(const char* ssid, const char* pass);
bool WIFI_sta_disconnect(void);
bool WIFI_setMdns(char* pName);
bool WIFI_isConnected(void);

#ifdef __cplusplus
}
#endif
