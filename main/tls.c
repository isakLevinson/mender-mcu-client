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

static struct {
	mbedtls_ssl_config conf;
	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context ctr_drbg;
	mbedtls_x509_crt srvcert;
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
			ERROR("mbedtls_ssl_handshake -%x\n", -ret);
			xSemaphoreGive(g_tls.mutex);
			goto reset;
		}
	}

	const mbedtls_x509_crt* client_cert = mbedtls_ssl_get_peer_cert(ssl);
	char cn[256];
	const mbedtls_x509_name* name = &client_cert->subject;
#if 1
	if (client_cert) {
		while (name) {
			if (MBEDTLS_OID_CMP(MBEDTLS_OID_AT_CN, &name->oid) == 0) {
				memcpy(cn, name->val.p, name->val.len);
				cn[name->val.len] = '\0';
				INFO("Client CN: %s\n", cn);
				break;
			}
			name = name->next;
		}
	} else {
		WARN("no client certificate received\n");
	}
#endif
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

static bool _taskInit(mbedtls_ssl_context* ssl, mbedtls_net_context* listen_fd, mbedtls_net_context* client_fd, char* port)
{
	int ret;
	mbedtls_net_init(listen_fd);
	mbedtls_net_init(client_fd);

	mbedtls_ssl_init(ssl);
	if ((ret = mbedtls_ssl_setup(ssl, &g_tls.conf)) != 0) {
		ERROR("mbedtls_ssl_setup %d\n", ret);
		return false;
	}

	if ((ret = mbedtls_net_bind(listen_fd, NULL, port, MBEDTLS_NET_PROTO_TCP)) != 0) {
		ERROR("mbedtls_net_bind %d\n", ret);
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

static uint8_t _createSanExt(char* cn_list[], uint8_t size, uint8_t* buf)
{
	uint8_t     i;
	uint8_t*    pBuf = buf + 2;
	uint8_t     len;

	for (i = 0; i < size; i++) {
		len = strlen(cn_list[i]);
		*pBuf++ = 0x82;
		*pBuf++ = len;
		memcpy(pBuf, cn_list[i], len);
		pBuf += len;
	}

	buf[0] = 0x30;
	buf[1] = pBuf - buf - 2;

	return pBuf - buf;
}

static bool _create_csr(mbedtls_pk_context* pKey, unsigned char* csr_buf, size_t size)
{
	int ret;
	mbedtls_x509write_csr csr;
	char    errStr[256];
	char    subject[64];
	char*   sn;
	unsigned char san_ext[256];
	uint8_t san_length;
	char    sn_local[64];

	ret = FACTORY_get(factory_id_sn, &sn);
	if (!ret) {
		ERROR("SN not set\n");
		return false;
	}
	sprintf(subject, "CN=%s", sn);
	sprintf(sn_local, "%s.local", sn);

	char* dns_list[] = {
		sn,
		sn_local,
	};

	san_length = _createSanExt(dns_list, 2, san_ext);
	INFO_BUF("san_ext",	PRINT_BUF_STYLE_ASC_HEX_SIZE_NL, san_ext, san_length);

	mbedtls_x509write_csr_init(&csr);

	// Setup CSR
	mbedtls_x509write_csr_set_md_alg(&csr, MBEDTLS_MD_SHA256);
	mbedtls_x509write_csr_set_key(&csr, &g_tls.pkey);

	mbedtls_x509write_csr_set_subject_name(&csr, subject);
	ret = mbedtls_x509write_csr_set_extension(&csr, MBEDTLS_OID_SUBJECT_ALT_NAME, MBEDTLS_OID_SIZE(MBEDTLS_OID_SUBJECT_ALT_NAME), 1, san_ext, san_length);
	if (ret != 0) {
		mbedtls_strerror(ret, errStr, sizeof(errStr));
		ERROR("mbedtls_x509write_csr_set_extension: -0x%04X %s\n", -ret, errStr);
		return false;
	}


	memset(csr_buf, 0, size);
	ret = mbedtls_x509write_csr_pem(&csr, csr_buf, size, mbedtls_ctr_drbg_random, &g_tls.ctr_drbg);
	if (ret < 0) {
		mbedtls_strerror(ret, errStr, sizeof(errStr));
		ERROR("mbedtls_x509write_csr_pem: -0x%04X %s\n", -ret, errStr);
		return false;
	}

	mbedtls_x509write_csr_free(&csr);

	return true;
}

static bool _voultCreateCsrJson(char* csr, char* ttl, char* json)
{
    char* pJson = json;

    pJson += sprintf(pJson, "{\"csr\":\"");

    while ('\0' != *csr) {
        switch (*csr) {
            case '\n':
                *pJson++ = '\\';
                *pJson++ = 'n';
                break;

            default:
                *pJson++ = *csr;
        }
        csr++;
    }

    pJson += sprintf(pJson, "\",\"ttl\":\"%s\"}", ttl);

    return true;
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

	ret = NVS_set(nvs_id_key, buf);
	if (!ret) {
		ERROR("NVS_set failed\n");
		return false;
	}

	return true;
}

static bool _tlsInit(void)
{
	int ret;
	const char* pers = "ssl_server";
	static char certificate[2048];
	static char key[2048];

	mbedtls_ssl_config_init(&g_tls.conf);
#if defined(MBEDTLS_SSL_CACHE_C)
	mbedtls_ssl_cache_init(&g_tls.cache);
#endif
	mbedtls_x509_crt_init(&g_tls.srvcert);
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
		return false;
	}

	INFO("Loading the server cert and key\n");
	NVS_get(nvs_id_certificate,  certificate, sizeof(certificate));
	uint32_t    cert_len = strlen(certificate) + 1;

	NVS_get(nvs_id_key,  key, sizeof(key));
	uint32_t    key_len = strlen(key) + 1;

	char* cacert_pem;
	FACTORY_get(factory_id_ca_certificate, &cacert_pem);
	int cacert_len = strlen(cacert_pem) + 1;

	ret = mbedtls_x509_crt_parse(&g_tls.srvcert, (unsigned char*)certificate, cert_len);
	if (ret != 0) {
		ERROR("mbedtls_x509_crt_parse returned %d\n", ret);
		return false;
	}

	ret = mbedtls_x509_crt_parse(&g_tls.srvcert, (const unsigned char*) cacert_pem, cacert_len);
	if (ret != 0) {
		ERROR("mbedtls_x509_crt_parse returned %d\n", ret);
		return false;
	}

	ret =  mbedtls_pk_parse_key(&g_tls.pkey, (unsigned char*)key, key_len, NULL, 0, mbedtls_ctr_drbg_random, &g_tls.ctr_drbg);
	if (ret != 0) {
		ERROR("mbedtls_pk_parse_key returned %d\n", ret);
		return false;
	}

	INFO("ok\n");

	if ((ret = mbedtls_ssl_config_defaults(&g_tls.conf,
	                MBEDTLS_SSL_IS_SERVER,
	                MBEDTLS_SSL_TRANSPORT_STREAM,
	                MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
		ERROR("mbedtls_ssl_config_defaults %d\n", ret);
		return false;
	}

	//g_tls.conf.private_read_timeout = 1000;

	mbedtls_ssl_conf_rng(&g_tls.conf, mbedtls_ctr_drbg_random, &g_tls.ctr_drbg);
	mbedtls_ssl_conf_dbg(&g_tls.conf, my_debug, stdout);

#if defined(MBEDTLS_SSL_CACHE_C)
	mbedtls_ssl_conf_session_cache(&g_tls.conf, &g_tls.cache, mbedtls_ssl_cache_get, mbedtls_ssl_cache_set);
#endif

	mbedtls_ssl_conf_ca_chain(&g_tls.conf, g_tls.srvcert.next, NULL);
	if ((ret = mbedtls_ssl_conf_own_cert(&g_tls.conf, &g_tls.srvcert, &g_tls.pkey)) != 0) {
		ERROR("mbedtls_ssl_conf_own_cert returned %d\n", ret);
		return false;
	}

	mbedtls_ssl_conf_authmode(&g_tls.conf, MBEDTLS_SSL_VERIFY_OPTIONAL);

	return true;
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

static void _print_cert_dates(const mbedtls_x509_crt* cert)
{
	const mbedtls_x509_time* from = &cert->valid_from;
	const mbedtls_x509_time* to   = &cert->valid_to;

	INFO("%04d-%02d-%02d %02d:%02d:%02d - ", from->year, from->mon, from->day, from->hour, from->min, from->sec);
	INFO("%04d-%02d-%02d %02d:%02d:%02d\n", to->year, to->mon, to->day, to->hour, to->min, to->sec);
}

static bool dbgStatus(uint8_t argc, char** argv)
{
	int     ret;
	char    errStr[256];
	char*   caCert;
	char    certificate[2048];

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

	mbedtls_x509_crt ca_cert;

	mbedtls_x509_crt_init(&ca_cert);

	FACTORY_get(factory_id_ca_certificate, &caCert);

	if (caCert) {
		uint32_t len = strlen(caCert) + 1;

		ret = mbedtls_x509_crt_parse(&ca_cert, (unsigned char*)caCert, len);
		if (ret < 0) {
			mbedtls_strerror(ret, errStr, sizeof(errStr));
			ERROR("Failed to parse CA cert: -0x%04x %s\n", -ret, errStr);

			INFO("CA cert: %d\n", len);
			for (int i = 0; i < len; i++) {
				if (caCert[i] < 0x20) {
					INFO("(%02x)", caCert[i]);
				}
				INFO("%c", caCert[i]);
			}
			INFO("\n###\n");
		} else {
			INFO("CA  : ");
			_print_cert_dates(&ca_cert);
		}
	}

	INFO("cert: ");
	_print_cert_dates(&g_tls.srvcert);

	uint32_t flags;
	mbedtls_x509_crt_profile profile = mbedtls_x509_crt_profile_default;
	//ret = mbedtls_x509_crt_verify_with_profile(&g_tls.srvcert, &ca_cert, NULL, &profile, NULL, &flags, NULL, NULL);
	ret = mbedtls_x509_crt_verify(&g_tls.srvcert, &ca_cert, NULL, NULL, &flags, NULL, NULL);

	if (ret) {
		char buf[256];
		mbedtls_x509_crt_verify_info(buf, sizeof(buf), "", flags);
		INFO("Certificate verification failed: %s\n", buf);
	} else {
		INFO("Certificate verification SUCCESS.\n");
	}

	mbedtls_x509_crt_free(&ca_cert);

	return true;
}

static bool dbgGenKey(uint8_t argc, char** argv)
{
	int ret;

	ret = _genPrivateKey();
	if (!ret) {
		PRINT("genkey failed\n");
		return false;
	}

	return true;
}

static bool dbgStoreKey(uint8_t argc, char** argv)
{
	int ret;

	ret = _storePrivateKey(&g_tls.pkey);
	if (!ret) {
		PRINT("store key failed\n");
		return false;
	}

	return true;
}

static bool dbgCreateCsr(uint8_t argc, char** argv)
{
    bool    ret;
    char csr_buf[2048];
     char json[2048];
	ret = _create_csr(&g_tls.pkey, (unsigned char*)csr_buf, sizeof(csr_buf));
    if (!ret) {
        ERROR("_create_csr failed\n");
        return true;
    }

	PRINT("csr:\n%s\n", csr_buf);

    _voultCreateCsrJson(csr_buf, "720h", json);

	PRINT("json:\n%s\n", json);

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
        DEBUG_MENU_CMD("storeKey",  NULL,	NULL, dbgStoreKey)
		DEBUG_MENU_CMD("csr",       NULL,	NULL, dbgCreateCsr)
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*

bool TLS_init(void)
{
	DBG_TREE_add("/", g_menu);

	_init();
	_tlsInit();

	return true;
}

#endif
