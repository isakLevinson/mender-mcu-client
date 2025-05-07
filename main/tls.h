#pragma once

#include <stdbool.h>

#if USE_TLS

#include "esp_netif.h"

// Register WiFi functions
bool TLS_init(void);

#else
#define TLS_init()  true
#endif