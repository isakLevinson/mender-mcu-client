#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_netif.h"

// Register WiFi functions
bool TLS_init(void);

#ifdef __cplusplus
}
#endif
