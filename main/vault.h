#pragma once

#include <stdbool.h>

#if USE_VAULT

// Register WiFi functions
bool VAULT_init(void);
bool VAULT_checkExpiration(mbedtls_x509_time* from, mbedtls_x509_time* to, int32_t expirationAdvanceDays);


#else
#define VAULT_init()  		true
#endif