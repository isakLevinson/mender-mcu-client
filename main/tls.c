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
#include "esp_http_client.h"

#include "freertos/FreeRTOS.h"
#include "lwip/sockets.h"
#include "mbedtls/platform.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/x509.h"
#include "mbedtls/debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509_csr.h"
#include "mbedtls/esp_debug.h"
#include "mbedtls/error.h"
#include "mbedtls/oid.h"
#include "nvs.h"
#include "factory.h"

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

typedef struct {
	mbedtls_ssl_context* ssl;
	mbedtls_net_context* fd;
} cmd_ctx_arg_t;


typedef struct {
	void* data;
	size_t maxSize;
	size_t size;
} curl_data_t;

static struct {
	mbedtls_ssl_config conf;
	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context ctr_drbg;
	mbedtls_x509_crt cert;
	mbedtls_x509_crt ca_cert;
	mbedtls_pk_context pkey;
#if defined(MBEDTLS_SSL_CACHE_C)
	mbedtls_ssl_cache_context cache;
#endif

	SemaphoreHandle_t	mutex;

	mbedtls_net_context fd_cmd;
	mbedtls_net_context fd_stream;
	TimerHandle_t		kaTimer;
} g_tls;

static void my_debug(void* ctx, int level, const char* file, int line, const char* str)
{
	TRACE("SSLDBG: %s:%04d: %s", file, line, str);
}

static bool _cmdKa(uint8_t timeout)
{
	int ret;

	if (!timeout) {
		return false;
	}

	xTimerChangePeriod(g_tls.kaTimer, timeout * 100, 0);
	ret = xTimerStart(g_tls.kaTimer, 0);
	if (pdPASS != ret) {
		return false;
	}
	return true;
}

static bool _accept(mbedtls_ssl_context* ssl, mbedtls_net_context* listen_fd, mbedtls_net_context* client_fd, bool startKaTimer)
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

	xSemaphoreTake(g_tls.mutex, portMAX_DELAY);

	mbedtls_ssl_set_bio(ssl, client_fd, mbedtls_net_send, mbedtls_net_recv, NULL);
	INFO("accept ok\n");

	if (startKaTimer) {
		_cmdKa(30);
	}

	while ((ret = mbedtls_ssl_handshake(ssl)) != 0) {
		if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
			char* str = mbedtls_high_level_strerr(ret);
			ERROR("mbedtls_ssl_handshake -%x %s\n", -ret, str);
			xSemaphoreGive(g_tls.mutex);
			goto reset;
		}
	}

	const mbedtls_x509_crt* client_cert = mbedtls_ssl_get_peer_cert(ssl);

	if (client_cert) {
		char cn[256];
		const mbedtls_x509_name* name = &client_cert->subject;

		uint32_t flags = mbedtls_ssl_get_verify_result(ssl);
		if (flags != 0) {
			char vrfy_buf[512];
			mbedtls_x509_crt_verify_info(vrfy_buf, sizeof(vrfy_buf), "", flags);
			ERROR("Certificate verification failed:\n%s", vrfy_buf);

			ret = mbedtls_x509_crt_verify(client_cert, &g_tls.ca_cert, NULL, NULL, &flags, NULL, NULL);
			if (ret != 0) {
				char vrfy_buf[512];
				mbedtls_x509_crt_verify_info(vrfy_buf, sizeof(vrfy_buf), "", flags);
				ERROR("Manual cert verification failed:\n%s", vrfy_buf);
			} else {
				INFO("But manual cert verification successful\n");
			}
		} else {
			INFO("Certificate verified successfully!\n");
		}

		while (name) {
			if (MBEDTLS_OID_CMP(MBEDTLS_OID_AT_CN, &name->oid) == 0) {
				memcpy(cn, name->val.p, name->val.len);
				cn[name->val.len] = '\0';
				INFO("Client CN: %s\n", cn);
				break;
			}
			name = name->next;
		}

		mbedtls_x509_time* exp = &client_cert->valid_to;
		INFO("Certificate expires on: %04d-%02d-%02d %02d:%02d:%02d\n", exp->year, exp->mon, exp->day, exp->hour, exp->min, exp->sec);
	} else {
		WARN("no client certificate received\n");
	}

	//	INFO("cert:%x\n", client_cert);
	//INFO("len: %d\n", name->oid.len);
	//	while (name) {
	//		//INFO("len: %d\n", name->oid.len);
	//
	//		name = name->next;
	//	}
	xSemaphoreGive(g_tls.mutex);

	INFO("handshake ok\n");
	return true;
}

static bool _write(mbedtls_ssl_context* ssl, void* buf, int len)
{
	int ret;

	while ((ret = mbedtls_ssl_write(ssl, buf, len)) <= 0) {
		if (ret == MBEDTLS_ERR_NET_CONN_RESET) {
			WARN("peer closed the connection\n");
			return false;
		}

		if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
			WARN("mbedtls_ssl_write returned %d\n", ret);
			//mbedtls_print_error_msg(ret);
			return false;
		}
	}

	return true;
}

static bool _cmdWrite(void* pArg, void* i_pBuf, uint16_t size)
{
	bool ret;

	cmd_ctx_arg_t* ctxarg = (cmd_ctx_arg_t*)pArg;

	TRACE_BUF("_cmdWrite",	PRINT_BUF_STYLE_HEX_SIZE_NL, i_pBuf, size);

	if (ctxarg->fd->fd < 0) {
		WARN("socket is closed\n");
		return false;
	}

	ret = _write(ctxarg->ssl, i_pBuf, size);
	return ret;
}


static bool _genPrivateKey(void)
{
	int     ret;
	char    errStr[256];

	memset(&g_tls.pkey, 0, sizeof(g_tls.pkey));

	ret = mbedtls_pk_setup(&g_tls.pkey, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
	if (ret != 0) {
		mbedtls_strerror(ret, errStr, sizeof(errStr));
		ERROR("mbedtls_pk_setup -0x%04x %s", -ret, errStr);
		return false;
	}
	INFO("mbedtls_pk_setup ok\n");

	ret = mbedtls_rsa_gen_key(mbedtls_pk_rsa(g_tls.pkey), mbedtls_ctr_drbg_random, &g_tls.ctr_drbg, 2048, 65537);
	if (ret != 0) {
		mbedtls_strerror(ret, errStr, sizeof(errStr));
		ERROR("mbedtls_rsa_gen_key -0x%04x %s", -ret, errStr);
		return false;
	}
	INFO("mbedtls_rsa_gen_key ok\n");

	return true;
}

static bool _storePrivateKey(mbedtls_pk_context* pkey)
{
	int     ret;
	char    errStr[256];
	char    buf[2048];

	ret = mbedtls_pk_write_key_pem(&g_tls.pkey, (unsigned char*)buf, sizeof(buf));
	if (ret != 0) {
		mbedtls_strerror(ret, errStr, sizeof(errStr));
		ERROR("mbedtls_pk_write_key_pem -0x%04x %s", -ret, errStr);
		return false;
	}

	INFO("key: %s\n", buf);

	ret = CFG_set(cfg_id_cert_key, buf);
	if (!ret) {
		ERROR("CFG_set failed\n");
		return false;
	}

	return true;
}

static bool _genAndStorePrivateKey(void)
{
	bool	ret;
	char    buf[2048];

	ret = CFG_get(cfg_id_cert_key, buf, sizeof(buf));
	if (ret) {
		INFO("key present. no need to generate\n");
		return true;
	}

	ret = _genPrivateKey();
	if (!ret) {
		ERROR("_genPrivateKey failed\n");
		return false;
	}

	ret = _storePrivateKey(&g_tls.pkey);
	if (!ret) {
		ERROR("_storePrivateKey failed\n");
		return false;
	}

	INFO("stored new key\n");

	return true;
}

static bool _taskInit(mbedtls_ssl_context* ssl, mbedtls_net_context* listen_fd, mbedtls_net_context* client_fd, char* port)
{
	int ret;
	mbedtls_net_init(listen_fd);
	mbedtls_net_init(client_fd);

	mbedtls_ssl_init(ssl);
	if ((ret = mbedtls_ssl_setup(ssl, &g_tls.conf)) != 0) {
		char* str = mbedtls_high_level_strerr(ret);
		ERROR("mbedtls_ssl_setup %x %s\n", -ret, str);
		INFO("exiting task\n");
		return false;
	}

	if ((ret = mbedtls_net_bind(listen_fd, NULL, port, MBEDTLS_NET_PROTO_TCP)) != 0) {
		char* str = mbedtls_high_level_strerr(ret);
		ERROR("mbedtls_net_bind %d %s\n", ret, str);
		return false;
	}

	mbedtls_net_free(client_fd);

	return true;
}

static void _taskCmd(void* arg)
{
	int ret;
	int len;
	unsigned char buf[1024];

	mbedtls_ssl_context ssl;
	mbedtls_net_context listen_fd;

	cmd_ctx_arg_t ctxarg = {
		.ssl	= &ssl,
		.fd		= &g_tls.fd_cmd,
	};

	CMD_CONTEXT context = {
		.p_cbSend   = _cmdWrite,
		.p_cbKa		= _cmdKa,
		.pArg       = &ctxarg,
	};

	//_genAndStorePrivateKey();

	ret = _taskInit(&ssl, &listen_fd, &g_tls.fd_cmd, TLS_CMD_PORT);
	if (!ret) {
		goto exit;
	}

	while (true) {
		vTaskDelay(100);

		ret = _accept(&ssl, &listen_fd, &g_tls.fd_cmd, true);
		if (!ret) {
			continue;
		}

		INFO("cmd connected\n");

		do {
			len = sizeof(buf) - 1;
			memset(buf, 0, sizeof(buf));
			ret = mbedtls_ssl_read(&ssl, buf, len);

			if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
				//TRACE("#1 %d\n", ret);
				continue;
			}
			//TRACE("#2 %d\n", ret);

			if (ret <= 0) {
				switch (ret) {
					case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
						INFO("connection was closed gracefully\n");
						break;

					case MBEDTLS_ERR_NET_CONN_RESET:
						INFO("connection was reset by peer\n");
						break;

					default:
						WARN("mbedtls_ssl_read returned -0x%x\n", (unsigned int) - ret);
						break;
				}
				INFO("cmd disconnected\n");
				ret = xTimerStop(g_tls.kaTimer, 0);
				break;
			}

			len = ret;
			TRACE_BUF("cmd",	PRINT_BUF_STYLE_HEX_SIZE_NL, buf, len);

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

	cmd_ctx_arg_t ctxarg = {
		.ssl	= &ssl,
		.fd		= &g_tls.fd_stream,
	};

	CMD_CONTEXT context = {
		.p_cbSend	= _cmdWrite,
		.pArg       = &ctxarg,
	};
	CMD_setStreamContext(&context);

	ret = _taskInit(&ssl, &listen_fd, &g_tls.fd_stream, TLS_STREAM_PORT);
	if (!ret) {
		goto exit;
	}

	while (true) {
		vTaskDelay(100);

		ret = _accept(&ssl, &listen_fd, &g_tls.fd_stream, false);
		if (!ret) {
			continue;
		}

		INFO("stream connected\n");

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
						WARN("mbedtls_ssl_read returned -0x%x\n", (unsigned int) - ret);
						break;
				}
				INFO("stream disconnected\n");
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

static void _kaTimerCb(TimerHandle_t pxTimer)
{
	INFO("_kaTimerCb\n");

	mbedtls_net_free(&g_tls.fd_cmd);
	mbedtls_net_free(&g_tls.fd_stream);
}

static bool _tlsInit(void)
{
	int			ret;
	const char* pers = "ssl_server";
	char*		buf = NULL;
	uint32_t	len;

	mbedtls_ssl_config_init(&g_tls.conf);
#if defined(MBEDTLS_SSL_CACHE_C)
	mbedtls_ssl_cache_init(&g_tls.cache);
#endif
	mbedtls_x509_crt_init(&g_tls.cert);
	mbedtls_x509_crt_init(&g_tls.ca_cert);

	mbedtls_pk_init(&g_tls.pkey);
	mbedtls_entropy_init(&g_tls.entropy);
	mbedtls_ctr_drbg_init(&g_tls.ctr_drbg);

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

	if ((ret = mbedtls_ctr_drbg_seed(&g_tls.ctr_drbg, mbedtls_entropy_func, &g_tls.entropy,
	                (const unsigned char*) pers,
	                strlen(pers))) != 0) {
		ERROR("mbedtls_ctr_drbg_seed returned %d\n", ret);
		goto err;
	}

	buf = calloc(1, 2048);
	INFO("Loading private key\n");
	ret = CFG_get(cfg_id_cert_key,  buf, 2048);
	if (!ret) {
		ERROR("key not present. will generate later\n");
		goto err;
	}

	ret =  mbedtls_pk_parse_key(&g_tls.pkey, (unsigned char*)buf, strlen(buf) + 1, NULL, 0, mbedtls_ctr_drbg_random, &g_tls.ctr_drbg);
	if (ret != 0) {
		ERROR("mbedtls_pk_parse_key returned %d\n", ret);
		goto err;
	}

	INFO("Loading CA cert\n");
	ret = CFG_get(cfg_id_ca_pem, buf, 2048);
	if (!ret) {
		ERROR("CA not present. Fatal !!!\n");
		goto err;
	}

	ret = mbedtls_x509_crt_parse(&g_tls.ca_cert, (const unsigned char*) buf, strlen(buf) + 1);
	if (ret != 0) {
		ERROR("mbedtls_x509_crt_parse returned %d\n", ret);
		goto err;
	}

	INFO("Loading cert\n");
	ret = CFG_get(cfg_id_cert_pem,  buf, 2048);
	if (!ret) {
		ERROR("certificate not present. will request later\n");
		goto err;
	}

	ret = mbedtls_x509_crt_parse(&g_tls.cert, (unsigned char*)buf, strlen(buf) + 1);
	if (ret != 0) {
		ERROR("mbedtls_x509_crt_parse returned %d\n", ret);
		goto err;
	}

	if ((ret = mbedtls_ssl_config_defaults(&g_tls.conf, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
		ERROR("mbedtls_ssl_config_defaults %d\n", ret);
		goto err;
	}

	//g_tls.conf.private_read_timeout = 1000;

	mbedtls_ssl_conf_rng(&g_tls.conf, mbedtls_ctr_drbg_random, &g_tls.ctr_drbg);
	mbedtls_ssl_conf_dbg(&g_tls.conf, my_debug, stdout);

#if defined(MBEDTLS_SSL_CACHE_C)
	mbedtls_ssl_conf_session_cache(&g_tls.conf, &g_tls.cache, mbedtls_ssl_cache_get, mbedtls_ssl_cache_set);
#endif

	mbedtls_ssl_conf_ca_chain(&g_tls.conf, g_tls.ca_cert.next, NULL);
	ret = mbedtls_ssl_conf_own_cert(&g_tls.conf, &g_tls.cert, &g_tls.pkey);
	if (ret != 0) {
		ERROR("mbedtls_ssl_conf_own_cert returned %d\n", ret);
		goto err;
	}

	mbedtls_ssl_conf_authmode(&g_tls.conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
	//mbedtls_ssl_conf_authmode(&g_tls.conf, MBEDTLS_SSL_VERIFY_REQUIRED);

	free(buf);

	INFO("TLS initialized\n");
	return true;

err:
	if (buf) {
		free(buf);
	}
	return false;
}

static esp_err_t _http_event_handler(esp_http_client_event_t* evt)
{
	TRACE("_http_event_handler: ");

	curl_data_t*	userData = (curl_data_t*)evt->user_data;
	if (!userData) {
		return ESP_ERR_INVALID_ARG;
	}

	if (!userData->data) {
		return ESP_ERR_INVALID_ARG;
	}

	if (!userData->maxSize) {
		return ESP_ERR_INVALID_ARG;
	}

	switch (evt->event_id) {
		case HTTP_EVENT_ERROR:
			TRACE("HTTP_EVENT_ERROR");
			break;
		case HTTP_EVENT_ON_CONNECTED:
			TRACE("HTTP_EVENT_ON_CONNECTED");
			break;
		case HTTP_EVENT_HEADER_SENT:
			TRACE("HTTP_EVENT_HEADER_SENT");
			break;
		case HTTP_EVENT_ON_HEADER:
			TRACE("HTTP_EVENT_ON_HEADER");
			TRACE_BUF("key", PRINT_BUF_STYLE_ASC_SIZE_NL, evt->header_key, 10);
			TRACE_BUF("val", PRINT_BUF_STYLE_ASC_SIZE_NL, evt->header_value, 10);
			break;
		case HTTP_EVENT_ON_DATA: {
			bool chunked = esp_http_client_is_chunked_response(evt->client);
			bool complete = esp_http_client_is_complete_data_received(evt->client);
			INFO("chunked:%d complete:%d\n", chunked, complete);
			TRACE_BUF("HTTP_EVENT_ON_DATA",	PRINT_BUF_STYLE_ASC_HEX_SIZE_NL, evt->data, evt->data_len);

			// The last byte in evt->user_data is kept for the NULL character in case of out-of-bound access.
			if (evt->data_len) {
				if (userData->size + evt->data_len > userData->maxSize) {
					WARN("no place to store %d bytes. stored %d/%d\n", evt->data_len, userData->size, userData->maxSize);
					return ESP_ERR_NO_MEM;
				}

				memcpy(userData->data + userData->size, evt->data, evt->data_len);
				userData->size += evt->data_len;
			}
		}
		break;

		case HTTP_EVENT_ON_FINISH:
			TRACE("HTTP_EVENT_ON_FINISH");
			break;

		case HTTP_EVENT_DISCONNECTED:
			TRACE("HTTP_EVENT_DISCONNECTED");
			break;

		case HTTP_EVENT_REDIRECT:
			TRACE("HTTP_EVENT_REDIRECT");
			break;
		default:
			TRACE("%d", evt->event_id);
	}

	TRACE("\n");

	return ESP_OK;
}

static bool _init(void)
{
	int ret;

	g_tls.mutex = xSemaphoreCreateMutex();

	g_tls.kaTimer = xTimerCreate("KA", 3000, pdFALSE, NULL, _kaTimerCb);
	if (!g_tls.kaTimer) {
		ERROR("xTimerCreate\n");
	}

	ret = xTaskCreate(_taskCmd, "tls_cmd", 8192, NULL, 3, NULL);
	if (ret != pdPASS) {
		ERROR("create task tls_cmd failed\n");
		return false;
	}

	ret = xTaskCreate(_taskStream, "tls_stream", 8192, NULL, 3, NULL);
	if (ret != pdPASS) {
		ERROR("create task tls_stream failed\n");
		return false;
	}

	return true;
}

bool	TLS_isConnected(void)
{
	if (g_tls.fd_cmd.fd < 0) {
		return false;
	}

	if (g_tls.fd_stream.fd < 0) {
		return false;
	}

	return true;
}

mbedtls_pk_context* TLS_getPkey(void)
{
	return &g_tls.pkey;
}

mbedtls_ctr_drbg_context* TLS_getDrbg(void)
{
	return &g_tls.ctr_drbg;
}

void TLS_getCerts(mbedtls_x509_crt** cert, mbedtls_x509_crt** ca)
{
	if (cert) {
		*cert = &g_tls.cert;
	}

	if (ca) {
		*ca = &g_tls.ca_cert;
	}
}

#define CURL_USE_CERTIFICATE	1
#define CURL_USE_CA				1

int TLS_curl(char* url, esp_http_client_method_t method, char* header_key, char* header_value, char* content, size_t contentSize, char* result, size_t maxResult)
{
	int		ret = true;
	esp_err_t err;
	int     read_len;
	int		i;
	esp_http_client_handle_t client	= NULL;
	char*	cert_pem				= NULL;
	char*	key_pem					= NULL;
	char*	ca_pem					= NULL;

	curl_data_t	userData = {
		.data = result,
		.maxSize = maxResult,
		.size = 0,
	};

	esp_http_client_config_t config = {
		.url = url,
		.method = method,
		.event_handler = _http_event_handler,
		.user_data = &userData,
		.buffer_size = maxResult,
	};

#if CURL_USE_CERTIFICATE
	config.skip_cert_common_name_check = true;

	cert_pem= malloc(2048);
	if (!cert_pem) {
		ret = -1;
		goto end;
	}

	ret = CFG_get(cfg_id_cert_pem, cert_pem, 2048);
	if (!ret) {
		ret = -1;
		goto end;
	}
	config.client_cert_pem = cert_pem;

	key_pem= malloc(2048);
	if (!key_pem) {
		ret = -1;
		goto end;
	}
	ret = CFG_get(cfg_id_cert_key, key_pem, 2048);
	if (!ret) {
		ret = -1;
		goto end;
	}
	config.client_key_pem = key_pem;

#endif

#if CURL_USE_CA
	ca_pem = malloc(2048);
	if (!ca_pem) {
		ret = -1;
		goto end;
	}

	ret = CFG_get(cfg_id_ca_pem, ca_pem, 2048);
	if (!ret) {
		ret = -1;
		goto end;
	}

	config.cert_pem = ca_pem;
#endif

	client = esp_http_client_init(&config);
	if (!client) {
		ERROR("esp_http_client_init failed\n");
		return -1;
	}

	if (header_key && header_value) {
		err = esp_http_client_set_header(client, header_key, header_value);
		if (err != ESP_OK) {
			ERROR("esp_http_client_set_header %x\n", err);
			ret = -1;
			goto end;
		}
	}

	if (content) {
		err = esp_http_client_set_post_field(client, content, contentSize);
		if (err != ESP_OK) {
			ERROR("esp_http_client_set_post_field %x\n", err);
			ret = -1;
			goto end;
		}
	}

	err = esp_http_client_perform(client);
	if (err != ESP_OK) {
		ERROR("esp_http_client_perform %x\n", err);
		ret = -1;
		goto end;
	}

	bool complete;
	i = 0;
	do {
		vTaskDelay(100);
		i++;
		if (i > 10) {
			break;
		}
		complete = esp_http_client_is_complete_data_received(client);
	} while (!complete);

	INFO("complete after %d\n", i);

	int content_len = esp_http_client_get_content_length(client);
	int chunk_len;

	bool chunked = esp_http_client_is_chunked_response(client);

	esp_http_client_get_chunk_length(client, &chunk_len);

	INFO("Status = %d chunked:%d content_len=%d chunk_len=%d\n", esp_http_client_get_status_code(client), chunked, content_len, chunk_len);
	result[userData.size] = '\0';
	TRACE_BUF("response",	PRINT_BUF_STYLE_ASC_HEX_SIZE_NL, result, userData.size);
	TRACE("response:\n%s\n", result);

	ret = userData.size;

end:
	esp_http_client_cleanup(client);

	if (cert_pem) {
		free(cert_pem);
	}
	if (key_pem) {
		free(key_pem);
	}

	if (ca_pem) {
		free(ca_pem);
	}

	return ret;
}

bool TLS_reload(void)
{
	bool	ret;
	ret = _tlsInit();
	return ret;
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
	} else {
		INFO("Certificate verified.\n");
	}

	INFO("Cipher suite is %s\n", mbedtls_ssl_get_ciphersuite(&ssl));
#endif
	return true;
}

static bool dbgTimer(uint8_t argc, char** argv)
{
	int ret;

	if (argc >= 2) {
		uint32_t period = strtol(argv[1], NULL, 10);
		xTimerChangePeriod(g_tls.kaTimer, period, 0);
	}

	ret = xTimerStart(g_tls.kaTimer, 0);
	PRINT("%d\n", ret);
	return true;
}

static bool dbgClose(uint8_t argc, char** argv)
{
	bool	isCmd = false;
	bool	isSteam = false;

	if (argc < 2) {
		isCmd	= true;
		isSteam	= true;
	} else {
		if (argv[1][0] == 'c') {
			isCmd = true;
		}
		if (argv[1][0] == 's') {
			isSteam = true;
		}
	}

	if (isCmd) {
		mbedtls_net_free(&g_tls.fd_cmd);
	}
	if (isSteam) {
		mbedtls_net_free(&g_tls.fd_stream);
	}
	return true;
}

static bool dbgStatus(uint8_t argc, char** argv)
{
	int     ret;
	char    errStr[256];

	PRINT("cmd    : %d\n", g_tls.fd_cmd.fd);
	PRINT("stresam: %d\n", g_tls.fd_stream.fd);

	BaseType_t timerState = xTimerIsTimerActive(g_tls.kaTimer);
	uint32_t expiration = xTimerGetExpiryTime(g_tls.kaTimer);
	int32_t	ticks = xTaskGetTickCount();
	if (timerState) {
		PRINT("timer active: %d %d %d\n", timerState, expiration, expiration - ticks);
	} else {
		PRINT("timer not active\n");
	}

	return true;
}



static bool dbgGenKey(uint8_t argc, char** argv)
{
	int		ret;
	bool	generate	= false;
	bool	autoGen		= false;
	bool	store		= false;

// *INDENT-OFF*
	ARGS_ENTRY_BEGIN(args)
		ARGS_ENTRY("g",		ARGS_TYPE_SWITCH,	0,	"force generate",				&generate)
		ARGS_ENTRY("a",		ARGS_TYPE_SWITCH,	0,	"generate if not present",		&autoGen)
		ARGS_ENTRY("s",		ARGS_TYPE_SWITCH,	0,	"store",						&store)
	ARGS_ENTRY_END()
// *INDENT-ON*

	ret = ARGS_readValues(argc, argv, args, NULL, NULL);
	if (!ret) {
		return false;
	}

	if (autoGen) {
		store = false;
	}

	if (generate) {
		ret = _genPrivateKey();
		if (!ret) {
			PRINT("genkey failed\n");
			return false;
		}
	}

	if (autoGen) {
		_genAndStorePrivateKey();
	}

	if (store) {
		ret = _storePrivateKey(&g_tls.pkey);
		if (!ret) {
			PRINT("store key failed\n");
			return false;
		}
	}

	return true;
}


static bool dbgCurl(uint8_t argc, char** argv)
{
	bool	ret;
	int		resultSize;
	esp_http_client_method_t	method = HTTP_METHOD_GET;
	char*	url;
	char* 	data = NULL;
	size_t	data_len = 0;
	char*	header_key		= NULL;
	char*	header_value	= NULL;
	bool	isPost			= false;
	char	http_result[2048];

// *INDENT-OFF*
	ARGS_ENTRY_BEGIN(args)
		ARGS_ENTRY("k",		ARGS_TYPE_STRING,	0,	"header key",	&header_key)
		ARGS_ENTRY("v",		ARGS_TYPE_STRING,	0,	"header value",	&header_value)
		ARGS_ENTRY("p",		ARGS_TYPE_SWITCH,	0,	"post",			&isPost)
		ARGS_ENTRY(NULL,	ARGS_TYPE_STRING,	0,	"url",   		&url)
		ARGS_ENTRY(NULL,	ARGS_TYPE_STRING,	0,	"data",   		&data)
	ARGS_ENTRY_END()
// *INDENT-ON*

	ret = ARGS_readValues(argc, argv, args, NULL, NULL);
	if (!ret) {
		return false;
	}

	if (!url) {
		return false;
	}

	if (data) {
		data_len = strlen(data);
		isPost = true;
	}

	if (isPost) {
		method = HTTP_METHOD_POST;
	}

	resultSize = TLS_curl(url, method, header_key, header_value,  data, data_len, http_result, sizeof(http_result));

	PRINT_BUF("response",	PRINT_BUF_STYLE_ASC_HEX_SIZE_NL, http_result, resultSize);
	//PRINT("response:\n%s\n", http_client_result);

	return true;
}

static bool dbgInit(uint8_t argc, char** argv)
{
	_tlsInit();
	return true;
}

// *INDENT-OFF*
DEBUG_MENU_START(g_menu)
	DEBUG_MENU_DIR("tls", NULL)
		DEBUG_MENU_CMD("status",    NULL,	NULL, dbgStatus)
		DEBUG_MENU_CMD("init",      NULL,	NULL, dbgInit)
		DEBUG_MENU_CMD("connect",   NULL,	NULL, dbgConnect)
		DEBUG_MENU_CMD("close",  	NULL,	NULL, dbgClose)
		DEBUG_MENU_CMD("timer",     NULL,	NULL, dbgTimer)
        DEBUG_MENU_CMD("genKey",    NULL,	NULL, dbgGenKey)
		DEBUG_MENU_CMD("curl",		NULL,	NULL, dbgCurl)
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*

bool TLS_init(void)
{
	DBG_TREE_add("/", g_menu);

	_tlsInit();
	_init();

	return true;
}

#endif
