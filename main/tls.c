#define DEF_DBG_MODULE	DBG_MODULE_TLS

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include <esp_event.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "lwip/sockets.h"
#include "mbedtls/platform.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/esp_debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#ifdef CONFIG_MBEDTLS_SSL_PROTO_TLS1_3
#include "psa/crypto.h"
#endif
#include "esp_crt_bundle.h"

#include "cmd.h"
#include "wifi.h"
#include "config.h"
#include "tls.h"

#define WEB_SERVER "192.168.1.100"
#define WEB_PORT	"2000"
mbedtls_entropy_context entropy;
mbedtls_ctr_drbg_context ctr_drbg;
mbedtls_ssl_context ssl;
mbedtls_x509_crt cacert;
mbedtls_ssl_config conf;
mbedtls_net_context server_fd;

mbedtls_net_context listen_fd;
mbedtls_net_context client_fd;


static void _task(void* arg)
{
	int	ret;
	mbedtls_net_context listen_fd;
	mbedtls_net_context client_fd;
    mbedtls_ssl_context ssl;
	mbedtls_ssl_config conf;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
	mbedtls_x509_crt cacert;

	const char *pers = "ssl_server";
	uint8_t buf[2048];
	int		len;

	INFO("TLS task\n");

	mbedtls_net_init(&listen_fd);
    mbedtls_net_init(&client_fd);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
	mbedtls_x509_crt_init(&cacert);

	mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        ERROR("psa_crypto_init %d\n", status);
        goto exit;
    }

	mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);

    if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
		(const unsigned char *) pers, strlen(pers))) != 0) {
			ERROR("mbedtls_ctr_drbg_seed %x\n", -ret);
			goto exit;
		}

	INFO("mbedtls_ctr_drbg_seed ok\n");
/*
	ret = esp_crt_bundle_attach(&conf);
    if(ret < 0)
    {
        ERROR("esp_crt_bundle_attach -0x%x", -ret);
        goto exit;
    }
*/
	if((ret = mbedtls_ssl_set_hostname(&ssl, "server")) != 0)
    {
        ERROR("mbedtls_ssl_set_hostname -0x%x", -ret);
        goto exit;
    }	

    if ((ret = mbedtls_net_bind(&listen_fd, NULL, "1000", MBEDTLS_NET_PROTO_TCP)) != 0) {
        ERROR("mbedtls_net_bind %x\n", -ret);
        goto exit;
    }

    if ((ret = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM,
											MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
			ERROR("mbedtls_ssl_config_defaults %x\n", -ret);
			goto exit;
	}

    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
    mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
    

	if ((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0) {
        ERROR("mbedtls_ssl_setup %x\n", -ret);
        goto exit;
    }

	while (true) {
		vTaskDelay(100);

		mbedtls_net_free(&client_fd);
		mbedtls_ssl_session_reset(&ssl);
	
		INFO("waiting for accept\n");

		if ((ret = mbedtls_net_accept(&listen_fd, &client_fd, NULL, 0, NULL)) != 0) {
			ERROR("mbedtls_net_accept %x\n", -ret);
			continue;
		}
		INFO("accept ok\n");

		mbedtls_ssl_set_bio(&ssl, &client_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

		do {
			ret = mbedtls_ssl_handshake(&ssl);
			INFO("mbedtls_ssl_handshake -%x\n", -ret);
		} while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

		if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
			WARN("mbedtls_ssl_handshake -%x\n", -ret);
			continue;
		}

		INFO("handshake ok\n");

		while(1) {
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
						WARN("connection was reset by peer\n");
						break;
	
					default:
						WARN("mbedtls_ssl_read -0x%x\n", -ret);
						break;
				}
	
				break;
			}
			len = ret;
			INFO_BUF("rx",	PRINT_BUF_STYLE_HEX_SIZE_NL, buf, len);

			if (ret > 0) {
				break;
			}
		}
	}

	exit:
	vTaskDelete(NULL);
}

static bool _init(void)
{
	int	ret;

	#ifdef CONFIG_MBEDTLS_SSL_PROTO_TLS1_3
    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to initialize PSA crypto, returned %d", (int) status);
        return;
    }
#endif

    mbedtls_ssl_init(&ssl);
    mbedtls_x509_crt_init(&cacert);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    INFO("Seeding the random number generator\n");

    mbedtls_ssl_config_init(&conf);

    mbedtls_entropy_init(&entropy);
    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0);
    if (ret) {
        ERROR("mbedtls_ctr_drbg_seed %d\n", ret);
        return false;
    }

    INFO("Attaching the certificate bundle...\n");

    ret = esp_crt_bundle_attach(&conf);
    if(ret < 0) {
        ERROR("esp_crt_bundle_attach -0x%x", -ret);
        return false;
    }

    INFO("Setting hostname for TLS session...\n");

     /* Hostname set here should match CN in server certificate */
    ret = mbedtls_ssl_set_hostname(&ssl, WEB_SERVER);
	if (ret) {
        ERROR("mbedtls_ssl_set_hostname returned -0x%x\n", -ret);
        return false;
    }

    INFO("Setting up the SSL/TLS structure...\n");

    ret = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret) {
        ERROR("mbedtls_ssl_config_defaults %d\n", ret);
        return false;
    }

    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
    mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
#ifdef CONFIG_MBEDTLS_DEBUG
    mbedtls_esp_enable_debug_log(&conf, CONFIG_MBEDTLS_DEBUG_LEVEL);
#endif

    ret = mbedtls_ssl_setup(&ssl, &conf);
    if (ret) {
        ERROR("mbedtls_ssl_setup -0x%x", -ret);
        return false;
    }

/*
	ret = xTaskCreate(_task, "tls", 16384, NULL, 3, NULL);
	if (ret != pdPASS) {
		ERROR("create task failed\n");
		return false;
	}
*/
	return true;
}

static bool dbgInit(uint8_t argc, char** argv)
{
	_init();
	return true;
}

static bool dbgConnect(uint8_t argc, char** argv)
{
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

	return true;
}

static bool dbgAccept(uint8_t argc, char** argv)
{
	int ret;
	int flags;
	char buf[512];

	mbedtls_net_init(&listen_fd);
    mbedtls_net_init(&client_fd);

    ret = mbedtls_net_bind(&listen_fd, NULL, "1000", MBEDTLS_NET_PROTO_TCP);
    if (ret) {
		ERROR("mbedtls_net_bind %x\n", -ret);
        return false;
    }

	INFO("waiting for accept\n");

	ret = mbedtls_net_accept(&listen_fd, &client_fd, NULL, 0, NULL);
	if (ret) {
		ERROR("mbedtls_net_accept %x\n", -ret);
		return false;
	}
	INFO("accept ok\n");

	mbedtls_ssl_set_bio(&ssl, &client_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

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

	return true;
}



static bool dbgStatus(uint8_t argc, char** argv)
{
	return true;
}

// *INDENT-OFF*
DEBUG_MENU_START(g_menu)
	DEBUG_MENU_DIR("tls", NULL)
		DEBUG_MENU_CMD("status",  NULL,		NULL, dbgStatus)
		DEBUG_MENU_CMD("init",	  NULL,		NULL, dbgInit)
		DEBUG_MENU_CMD("connect", NULL,		NULL, dbgConnect)
		DEBUG_MENU_CMD("accept",  NULL,		NULL, dbgAccept)
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*

bool TLS_init(void)
{
	DBG_TREE_add("/", g_menu);

	_init();
	return true;
}
