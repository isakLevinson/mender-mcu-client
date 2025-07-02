#pragma once

#include <stdbool.h>

#if USE_VAULT

// Register WiFi functions
bool VAULT_init(void);

#else
#define VAULT_init()  		true
#endif