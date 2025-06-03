#pragma once

#include <stdbool.h>

#if USE_TLS

#include "esp_netif.h"

// Register WiFi functions
bool TLS_init(void);
bool TLS_keepaliveRestart(uint16_t period);

#else
#define TLS_init()  true
#define TLS_keepaliveRestart(period)
#endif