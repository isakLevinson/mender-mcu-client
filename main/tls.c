#define DEF_DBG_MODULE	DBG_MODULE_TLS

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#if USE_TLS

#include <esp_event.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "lwip/sockets.h"
#include "mbedtls/platform.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/x509.h"
#include "mbedtls/debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/esp_debug.h"
#include "mbedtls/error.h"

#if defined(MBEDTLS_SSL_CACHE_C)
#include "mbedtls/ssl_cache.h"
#endif

#ifdef CONFIG_MBEDTLS_SSL_PROTO_TLS1_3
#include "psa/crypto.h"
#endif
#include "esp_crt_bundle.h"

#include "cmd.h"
#include "wifi.h"
#include "config.h"
#include "tls.h"
#include "cmd.h"

#define WEB_SERVER "192.168.1.100"
#define WEB_PORT	"2000"

static struct {
    mbedtls_ssl_config conf;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_x509_crt srvcert;
    mbedtls_pk_context pkey;
#if defined(MBEDTLS_SSL_CACHE_C)
    mbedtls_ssl_cache_context cache;
#endif
} g_ssl;

static void my_debug(void *ctx, int level, const char *file, int line, const char *str)
{   
	TRACE("SSLDBG: %s:%04d: %s", file, line, str);
}

static bool _accept(mbedtls_ssl_context *ssl, mbedtls_net_context *listen_fd, mbedtls_net_context *client_fd)
{
    int ret;

	reset:
    mbedtls_net_free(client_fd);
    mbedtls_ssl_session_reset(ssl);

    INFO("Waiting for a remote connection ...\n");

    if ((ret = mbedtls_net_accept(listen_fd, client_fd, NULL, 0, NULL)) != 0) {
        ERROR("mbedtls_net_accept %d\n", ret);
        return false;
    }

    mbedtls_ssl_set_bio(ssl, client_fd, mbedtls_net_send, mbedtls_net_recv, NULL);
    INFO("accept ok\n");

    INFO("Performing the SSL/TLS handshake...\n");

    while ((ret = mbedtls_ssl_handshake(ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            ERROR("mbedtls_ssl_handshake -%x\n", -ret);
            goto reset;
        }
    }

    INFO("handshake ok\n");
    return true;
}

static bool _write(mbedtls_ssl_context *ssl, void *buf, int len)
{
    int ret;

    while ((ret = mbedtls_ssl_write(ssl, buf, len)) <= 0) {
        if (ret == MBEDTLS_ERR_NET_CONN_RESET) {
            WARN("peer closed the connection\n");
            return false;
        }

        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            WARN("mbedtls_ssl_write returned %d\n", ret);
            return false;
        }
    }

    return true;
}

static bool _cmdWrite(void* pArg, void* i_pBuf, uint16_t size)
{
    bool ret;
    mbedtls_ssl_context *ssl = (mbedtls_ssl_context *)pArg;

    TRACE_BUF("_cmdWrite",	PRINT_BUF_STYLE_HEX_SIZE_NL, i_pBuf, size);

    ret = _write(ssl, i_pBuf, size);
    return ret;
}

static void _taskCmd(void* arg)
{
    int ret;
    int len;
    unsigned char buf[1024];

    mbedtls_ssl_context ssl;
    mbedtls_net_context listen_fd;
    mbedtls_net_context client_fd;

    CMD_CONTEXT context = {
        .p_cbSend   = _cmdWrite,
        .pArg       = &ssl,
    };

    mbedtls_net_init(&listen_fd);
    mbedtls_net_init(&client_fd);

    mbedtls_ssl_init(&ssl);
    if ((ret = mbedtls_ssl_setup(&ssl, &g_ssl.conf)) != 0) {
        ERROR("failed\n  ! mbedtls_ssl_setup returned %d\n", ret);
        goto exit;
    }

    if ((ret = mbedtls_net_bind(&listen_fd, NULL, "1000", MBEDTLS_NET_PROTO_TCP)) != 0) {
        ERROR("mbedtls_net_bind %d\n", ret);
        goto exit;
    }

    mbedtls_net_free(&client_fd);

    while (true) {
		vTaskDelay(100);

        _accept(&ssl, &listen_fd, &client_fd);

        do {
            len = sizeof(buf) - 1;
            memset(buf, 0, sizeof(buf));
            ret = mbedtls_ssl_read(&ssl, buf, len);
    
            if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
                continue;
            }
    
            if (ret <= 0) {
                switch (ret) {
                    case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
                        INFO("connection was closed gracefully\n");
                        break;
    
                    case MBEDTLS_ERR_NET_CONN_RESET:
                        INFO("connection was reset by peer\n");
                        break;
    
                    default:
                        WARN("mbedtls_ssl_read returned -0x%x\n", (unsigned int) -ret);
                        break;
                }
    
                break;
            }
    
            len = ret;
            INFO_BUF("cmd",	PRINT_BUF_STYLE_HEX_SIZE_NL, buf, len);
    
            CMD_processBuffer(&context, buf, len);
        } while (1);
    }

    exit:
    mbedtls_net_free(&listen_fd);
    vTaskDelete(NULL);
}

static void _taskStream(void* arg)
{
    int ret;
    int len;
    unsigned char buf[1024];

    mbedtls_ssl_context ssl;
    mbedtls_net_context listen_fd;
    mbedtls_net_context client_fd;

    CMD_CONTEXT context = {
        .p_cbSend   = _cmdWrite,
        .pArg       = &ssl,
    };
    CMD_setStreamContext(&context);

    mbedtls_net_init(&listen_fd);
    mbedtls_net_init(&client_fd);

    mbedtls_ssl_init(&ssl);
    if ((ret = mbedtls_ssl_setup(&ssl, &g_ssl.conf)) != 0) {
        ERROR("failed\n  ! mbedtls_ssl_setup returned %d\n", ret);
        goto exit;
    }

    if ((ret = mbedtls_net_bind(&listen_fd, NULL, "1001", MBEDTLS_NET_PROTO_TCP)) != 0) {
        ERROR("mbedtls_net_bind %d\n", ret);
        goto exit;
    }

    mbedtls_net_free(&client_fd);

    while (true) {
		vTaskDelay(100);

        _accept(&ssl, &listen_fd, &client_fd);

        do {
            len = sizeof(buf) - 1;
            memset(buf, 0, sizeof(buf));
            ret = mbedtls_ssl_read(&ssl, buf, len);
    
            if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
                continue;
            }
    
            if (ret <= 0) {
                switch (ret) {
                    case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
                        INFO("connection was closed gracefully\n");
                        break;
    
                    case MBEDTLS_ERR_NET_CONN_RESET:
                        INFO("connection was reset by peer\n");
                        break;
    
                    default:
                        WARN("mbedtls_ssl_read returned -0x%x\n", (unsigned int) -ret);
                        break;
                }
    
                break;
            }
    
            len = ret;
            INFO_BUF("stream",	PRINT_BUF_STYLE_ASC_SIZE_NL, buf, len);
    
            // echo back the buffer
            //_write(&ssl, buf, len);
        } while (1);
    }

    exit:
    mbedtls_net_free(&listen_fd);
    vTaskDelete(NULL);
}

static bool _sslInit(void)
{
    int ret;
    const char *pers = "ssl_server";

    mbedtls_ssl_config_init(&g_ssl.conf);
#if defined(MBEDTLS_SSL_CACHE_C)
    mbedtls_ssl_cache_init(&g_ssl.cache);
#endif
    mbedtls_x509_crt_init(&g_ssl.srvcert);
    mbedtls_pk_init(&g_ssl.pkey);
    mbedtls_entropy_init(&g_ssl.entropy);
    mbedtls_ctr_drbg_init(&g_ssl.ctr_drbg);
    
#if defined(MBEDTLS_USE_PSA_CRYPTO)
    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        mbedtls_fprintf(stderr, "Failed to initialize PSA Crypto implementation: %d\n",
                        (int) status);
        ret = MBEDTLS_ERR_SSL_HW_ACCEL_FAILED;
        goto exit;
    }
#endif /* MBEDTLS_USE_PSA_CRYPTO */

#if defined(MBEDTLS_DEBUG_C)
    mbedtls_debug_set_threshold(DEBUG_LEVEL);
#endif

    INFO("Seeding the random number generator...\n");

    if ((ret = mbedtls_ctr_drbg_seed(&g_ssl.ctr_drbg, mbedtls_entropy_func, &g_ssl.entropy,
                                     (const unsigned char *) pers,
                                     strlen(pers))) != 0) {
        ERROR("failed\n  ! mbedtls_ctr_drbg_seed returned %d\n", ret);
        return false;
    }

    INFO("Loading the server cert. and key...\n");
    extern const unsigned char server_cert_start[] asm("_binary_server_crt_start");
	extern const unsigned char server_cert_end[]   asm("_binary_server_crt_end");
	const uint8_t* servercert = server_cert_start;
	int servercert_len = server_cert_end - server_cert_start;

	extern const unsigned char prvtkey_pem_start[] asm("_binary_server_key_start");
	extern const unsigned char prvtkey_pem_end[]   asm("_binary_server_key_end");
	const uint8_t* prvtkey_pem = prvtkey_pem_start;
	int prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;

	extern const unsigned char ca_cert_start[] asm("_binary_ca_crt_start");
	extern const unsigned char ca_cert_end[]   asm("_binary_ca_crt_end");
	const uint8_t* cacert_pem = ca_cert_start;
	int cacert_len = ca_cert_end - ca_cert_start;

    ret = mbedtls_x509_crt_parse(&g_ssl.srvcert, (const unsigned char *) servercert, servercert_len);
    if (ret != 0) {
        ERROR("failed\n  !  mbedtls_x509_crt_parse returned %d\n", ret);
        return false;
    }

   ret = mbedtls_x509_crt_parse(&g_ssl.srvcert, (const unsigned char *) cacert_pem, cacert_len);
    if (ret != 0) {
        ERROR("failed\n  !  mbedtls_x509_crt_parse returned %d\n", ret);
        return false;
    }

    ret =  mbedtls_pk_parse_key(&g_ssl.pkey, (const unsigned char *) prvtkey_pem, prvtkey_len, NULL, 0,
                                mbedtls_ctr_drbg_random, &g_ssl.ctr_drbg);
    if (ret != 0) {
        ERROR("failed\n  !  mbedtls_pk_parse_key returned %d\n", ret);
        return false;
    }

    INFO("ok\n");

    INFO("Bind on https://localhost:4433/ ...\n");

    INFO("Setting up the SSL data....\n");

    if ((ret = mbedtls_ssl_config_defaults(&g_ssl.conf,
                                           MBEDTLS_SSL_IS_SERVER,
                                           MBEDTLS_SSL_TRANSPORT_STREAM,
                                           MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        ERROR("failed\n  ! mbedtls_ssl_config_defaults returned %d\n", ret);
        return false;
    }

    mbedtls_ssl_conf_rng(&g_ssl.conf, mbedtls_ctr_drbg_random, &g_ssl.ctr_drbg);
    mbedtls_ssl_conf_dbg(&g_ssl.conf, my_debug, stdout);

#if defined(MBEDTLS_SSL_CACHE_C)
    mbedtls_ssl_conf_session_cache(&g_ssl.conf, &g_ssl.cache, mbedtls_ssl_cache_get, mbedtls_ssl_cache_set);
#endif

    mbedtls_ssl_conf_ca_chain(&g_ssl.conf, g_ssl.srvcert.next, NULL);
    if ((ret = mbedtls_ssl_conf_own_cert(&g_ssl.conf, &g_ssl.srvcert, &g_ssl.pkey)) != 0) {
        ERROR("failed\n  ! mbedtls_ssl_conf_own_cert returned %d\n", ret);
        return false;
    }

    INFO("ok\n");
    return true;
}

static bool _init(void)
{
    int ret;

    _sslInit();

    ret = xTaskCreate(_taskCmd, "tls_cmd", 8192, NULL, 3, NULL);
	if (ret != pdPASS) {
		ERROR("create task failed\n");
		return false;
	}

    ret = xTaskCreate(_taskStream, "tls_stream", 8192, NULL, 3, NULL);
	if (ret != pdPASS) {
		ERROR("create task failed\n");
		return false;
	}


    return true;
}

static bool dbgConnect(uint8_t argc, char** argv)
{
#if 0
    int ret;
	int flags;
	char buf[512];

	mbedtls_net_init(&server_fd);

	INFO("Connecting to %s:%s...", WEB_SERVER, WEB_PORT);

	ret = mbedtls_net_connect(&server_fd, WEB_SERVER, WEB_PORT, MBEDTLS_NET_PROTO_TCP);
	if (ret) {
		ERROR("mbedtls_net_connect -%x", -ret);
		return false;
	}

	INFO("Connected.\n");

	mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

	INFO("Performing the SSL/TLS handshake...\n");

	while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
		if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
			ERROR("mbedtls_ssl_handshake -0x%x", -ret);
			return false;
		}
	}

	INFO("Verifying peer X.509 certificate...\n");

	if ((flags = mbedtls_ssl_get_verify_result(&ssl)) != 0) {
		/* In real life, we probably want to close connection if ret != 0 */
		WARN("Failed to verify peer certificate!\n");
		bzero(buf, sizeof(buf));
		mbedtls_x509_crt_verify_info(buf, sizeof(buf), "  ! ", flags);
		WARN("verification info: %s\n", buf);
	}
	else {
		INFO("Certificate verified.\n");
	}

	INFO("Cipher suite is %s\n", mbedtls_ssl_get_ciphersuite(&ssl));
#endif
	return true;
}

static bool dbgStatus(uint8_t argc, char** argv)
{
	return true;
}

// *INDENT-OFF*
DEBUG_MENU_START(g_menu)
	DEBUG_MENU_DIR("tls", NULL)
		DEBUG_MENU_CMD("status",      NULL,	NULL, dbgStatus)
		DEBUG_MENU_CMD("connect",     NULL,	NULL, dbgConnect)
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*

bool TLS_init(void)
{
    DBG_TREE_add("/", g_menu);

	_init();


    return true;
}

#endif
