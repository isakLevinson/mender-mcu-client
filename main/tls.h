#pragma once

#include <stdbool.h>

#if USE_TLS

#include "esp_netif.h"

// Register WiFi functions
bool	TLS_init(void);
bool	TLS_isConnected(void);

#else
#define TLS_init()  		true
#define TLS_keepaliveRestart(period)
#define TLS_isConnected		false
#endif