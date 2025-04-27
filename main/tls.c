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
#include "mbedtls/net_sockets.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "cmd.h"
#include "wifi.h"
#include "config.h"
#include "tls.h"



static void _task(void* arg)
{
	int	ret;
	mbedtls_net_context listen_fd;
	mbedtls_net_context client_fd;
    mbedtls_ssl_context ssl;
	mbedtls_ssl_config conf;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
	const char *pers = "ssl_server";
	uint8_t buf[2048];
	int		len;

	INFO("TLS task\n");

	mbedtls_net_init(&listen_fd);
    mbedtls_net_init(&client_fd);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);

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
			ERROR(" mbedtls_ctr_drbg_seed  %x\n", -ret);
			goto exit;
		}

	INFO("mbedtls_ctr_drbg_seed ok\n");

    if ((ret = mbedtls_net_bind(&listen_fd, NULL, "1000", MBEDTLS_NET_PROTO_TCP)) != 0) {
        ERROR("mbedtls_net_bind %x\n", -ret);
        goto exit;
    }

    if ((ret = mbedtls_ssl_config_defaults(&conf,
		MBEDTLS_SSL_IS_SERVER,
		MBEDTLS_SSL_TRANSPORT_STREAM,
		MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
		ERROR("mbedtls_ssl_config_defaults %x\n", -ret);
		goto exit;
	}

	if ((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0) {
        ERROR("mbedtls_ssl_setup %x\n", -ret);
        goto exit;
    }

	while (true) {
		vTaskDelay(100);

		mbedtls_net_free(&client_fd);
		mbedtls_ssl_session_reset(&ssl);
	
		INFO("waiting for accept\n");

		if ((ret = mbedtls_net_accept(&listen_fd, &client_fd,
			NULL, 0, NULL)) != 0) {
			ERROR("mbedtls_net_accept %x\n", -ret);
			goto exit;
		}
		INFO("accept ok\n");

		mbedtls_ssl_set_bio(&ssl, &client_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

		while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
			if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
				ERROR("mbedtls_ssl_handshake %x\n", ret);
				continue;
			}
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

	ret = xTaskCreate(_task, "tls", 16384, NULL, 3, NULL);
	if (ret != pdPASS) {
		ERROR("create task failed\n");
		return false;
	}
	return true;
}

bool TLS_init(void)
{
	_init();
	return true;
}
