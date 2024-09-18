#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_netif.h"


// Register WiFi functions
void register_wifi(void);
void initialise_wifi(void);
bool wifi_cmd_sta_join(const char *ssid, const char *pass);
esp_ip4_addr_t  wifi_getSelfIp(void);

bool    SER_sendUdp(void* i_pBuf, uint16_t len);
bool    SER_sendTcp(void* i_pBuf, uint16_t len);

#ifdef __cplusplus
}
#endif
