#pragma once

#include <stdbool.h>

#if USE_TLS

#include "esp_http_client.h"
#include "mbedtls/ssl.h"
#include "mbedtls/ctr_drbg.h"

// Register WiFi functions
bool	TLS_init(void);
bool	TLS_isConnected(void);
int     TLS_curl(char* url, esp_http_client_method_t method, char* header_key, char* header_value, char* content, size_t contentSize, char* result, size_t maxResult);
bool	TLS_reload(void);

mbedtls_pk_context*			TLS_getPkey(void);
mbedtls_ctr_drbg_context*	TLS_getDrbg(void);
void            			TLS_getCerts(mbedtls_x509_crt** cert, mbedtls_x509_crt** ca);

#else
#define TLS_init()  		true
#define TLS_keepaliveRestart(period)
#define TLS_isConnected		false
#define TLS_reload()		true
#define TLS_getPkey()		NULL
#define TLS_getDrbg()		NULL
#define TLS_getCert()		NULL
#endif