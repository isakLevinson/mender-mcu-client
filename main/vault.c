#define DEF_DBG_MODULE	DBG_MODULE_VAULT

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#if USE_TLS

#include <esp_event.h>
#include <esp_system.h>

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
#include "factory.h"
#include "config.h"
#include "tls.h"
#include "nvs.h"
#include "cJSON.h"


#define VAULT_ROLE_NAME		"brain-space"
#define VAULT_URL_LOGIN		"/v1/auth/approle/login"
#define VAULT_URL_RENEW		"/v1/pki_int/sign/" VAULT_ROLE_NAME
//#define VAULT_URL_RENEW		"/v1/pki_int/issue/" VAULT_ROLE_NAME

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
	char	sn[64];
	unsigned char san_ext[256];
	uint8_t san_length;
	char    sn_local[64];
	mbedtls_pk_context*	pkey = TLS_getPkey();
	mbedtls_ctr_drbg_context* drbg = TLS_getDrbg();

	ret = FACTORY_get(factory_id_sn, sn);
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
	mbedtls_x509write_csr_set_key(&csr, pkey);

	mbedtls_x509write_csr_set_subject_name(&csr, subject);
	ret = mbedtls_x509write_csr_set_extension(&csr, MBEDTLS_OID_SUBJECT_ALT_NAME, MBEDTLS_OID_SIZE(MBEDTLS_OID_SUBJECT_ALT_NAME), 1, san_ext, san_length);
	if (ret != 0) {
		mbedtls_strerror(ret, errStr, sizeof(errStr));
		ERROR("mbedtls_x509write_csr_set_extension: -0x%04X %s\n", -ret, errStr);
		return false;
	}

	memset(csr_buf, 0, size);
	ret = mbedtls_x509write_csr_pem(&csr, csr_buf, size, mbedtls_ctr_drbg_random, drbg);
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

static void _print_cert_dates(const mbedtls_x509_crt* cert)
{
	const mbedtls_x509_time* from = &cert->valid_from;
	const mbedtls_x509_time* to   = &cert->valid_to;

	INFO("%04d-%02d-%02d %02d:%02d:%02d - ", from->year, from->mon, from->day, from->hour, from->min, from->sec);
	INFO("%04d-%02d-%02d %02d:%02d:%02d\n", to->year, to->mon, to->day, to->hour, to->min, to->sec);
}


static bool _vaultLogin(char* o_pToken)
{
	bool	ret;
	char	url[128];
	char	data[1024];
	char	baseUrl[64];
	char	role[64];
	char	secret[64];
	char*	http_result;
	int		http_result_size;

	ret = FACTORY_get(factory_id_vault_url, baseUrl);
	if (!ret) {
		ERROR("vault url not set\n");
		return false;
	}

	ret = FACTORY_get(factory_id_vault_role, role);
	if (!ret) {
		ERROR("vault role not set\n");
		return false;
	}

	ret = FACTORY_get(factory_id_vault_secret, secret);
	if (!ret) {
		ERROR("vault secret not set\n");
		return false;
	}

	sprintf(url, "%s%s", baseUrl, VAULT_URL_LOGIN);
	sprintf(data, "{\"role_id\":\"%s\",\"secret_id\":\"%s\"}", role, secret);

	http_result_size = TLS_curl(url, HTTP_METHOD_POST, NULL, NULL,  data, strlen(data), &http_result);
	if (http_result_size < 0) {
		ERROR("TLS_curl failed\n");
		return false;
	}

	cJSON* root = cJSON_Parse(http_result);
	if (root == NULL) {
		ERROR("Failed to parse JSON\n");
		return false;
	}

	cJSON* auth = cJSON_GetObjectItem(root, "auth");
	if (!cJSON_IsObject(auth)) {
		ERROR("Missing or invalid 'auth' field\n");
		goto err;
	}

	// Access .certificate
	cJSON* token = cJSON_GetObjectItem(auth, "client_token");
	if (!cJSON_IsString(token)) {
		ERROR("Missing or invalid 'client_token' field\n");
		goto err;
	}
	INFO("TOKEN: %s\n", token->valuestring);
	if (o_pToken) {
		strcpy(o_pToken, token->valuestring);
	}

	cJSON_Delete(root);
	return true;

err:
	//INFO_BUF("response",	PRINT_BUF_STYLE_ASC_HEX_SIZE_NL, http_result, http_result_size);
	cJSON* errors = cJSON_GetObjectItem(root, "errors");
	if (cJSON_IsArray(errors)) {
		int size = cJSON_GetArraySize(errors);
		for (int i = 0; i < size; i++) {
			cJSON* item = cJSON_GetArrayItem(errors, i);
			if (cJSON_IsString(item)) {
				ERROR(" <%s> ", item->valuestring);
			}
		}
		//ERROR("\n%d\n", size);
	} else {
		ERROR("errors field is not an array\n");
		//		char *printed_json = cJSON_Print(root);  // Pretty print with indentation
		//			if (printed_json) {
		//				INFO("Full JSON Content:\n%s\n", printed_json);
		//				free(printed_json);
		//			}
	}

	cJSON_Delete(root);
	return false;
}

static bool _vaultRenew(char* token)
{
	bool    ret;
	esp_err_t err;
	char    csr_buf[2048];
	char    json[2048];
	int		resultSize;
	mbedtls_pk_context*	pkey = TLS_getPkey();
	char*	http_result;
	char	baseUrl[64];
	char	url[256];

	ret = FACTORY_get(factory_id_vault_url, baseUrl);
	if (!ret) {
		ERROR("vault url not set\n");
		return false;
	}

	sprintf(url, "%s%s", baseUrl, VAULT_URL_RENEW);
	INFO("url: %s\n", url);

	ret = _create_csr(pkey, (unsigned char*)csr_buf, sizeof(csr_buf));
	if (!ret) {
		ERROR("_create_csr failed\n");
		return true;
	}

	INFO("csr:\n%s\n", csr_buf);

	_voultCreateCsrJson(csr_buf, "720h", json);

	INFO("token: %s\n", token);
	//INFO("json:\n%s\n", json);

	resultSize = TLS_curl(url, HTTP_METHOD_POST, "X-Vault-Token", token, json, strlen(json), &http_result);
	if (resultSize < 0) {
		ERROR("TLS_curl failed\n");
		return false;
	}

	//	PRINT_BUF("response",	PRINT_BUF_STYLE_ASC_HEX_SIZE_NL, http_client_result, http_client_result_size);

	cJSON* root = cJSON_Parse(http_result);
	if (root == NULL) {
		ERROR("Failed to parse JSON\n");
		return false;
	}

	INFO_BUF("result",	PRINT_BUF_STYLE_ASC_HEX_SIZE_NL, http_result, resultSize);

	cJSON* data = cJSON_GetObjectItem(root, "data");
	if (!cJSON_IsObject(data)) {
		ERROR("Missing or invalid 'data' field\n");
		goto err;
	}

	// Access .certificate
	cJSON* cert = cJSON_GetObjectItem(data, "certificate");
	if (!cJSON_IsString(cert)) {
		ERROR("Missing or invalid 'certificate' field\n");
		goto err;
	}

	cJSON* ca = cJSON_GetObjectItem(data, "issuing_ca");
	if (ca) {
		INFO("CA:\n%s\n", ca->valuestring);
	}
	cJSON* chain = cJSON_GetObjectItem(data, "ca_chain");
	if (chain) {
		INFO("CHAIN\n");
		if (cJSON_IsArray(chain)) {
			int size = cJSON_GetArraySize(chain);
			INFO("CHAIN ARR size:%d\n", size);
			for (int i = 0; i < size; i++) {
				cJSON* item = cJSON_GetArrayItem(chain, i);
				if (cJSON_IsString(item)) {
					INFO("CHAIN %d: %s\n", i, item->valuestring);
				}
			}
			if (size >= 1) {
				cJSON* item = cJSON_GetArrayItem(chain, 0);
				if (cJSON_IsString(item)) {
					INFO("INTERMEDIATE:\n%s\n", item->valuestring);
					ret = CFG_set(cfg_id_inter_pem, item->valuestring);
					if (!ret) {
						ERROR("NVS_set failed\n");
						goto err;
					}
				}
			} else {
				ERROR("unexpected ca_chain size %d\n", size);
				goto err;
			}
		}
		//INFO_BUF("CHAIN",	PRINT_BUF_STYLE_ASC_HEX_SIZE_NL, chain->valuestring, strlen(chain->valuestring));
		//		INFO("CHAIN:\n%s\n", chain->valuestring);
	}
	cJSON* key = cJSON_GetObjectItem(data, "private_key");
	if (key) {
		INFO("PKEY\n");
		INFO_BUF("PKEY",	PRINT_BUF_STYLE_ASC_HEX_SIZE_NL, key->valuestring, strlen(key->valuestring));
		//		INFO("PKEY:\n%s\n", key->valuestring);
	}

	// Print certificate (like jq -r)
	INFO("CERT:\n%s\n", cert->valuestring);
	ret = CFG_set(cfg_id_cert_pem, cert->valuestring);
	if (!ret) {
		ERROR("NVS_set failed\n");
		goto err;
	}

	cJSON_Delete(root);

	ret = TLS_reload();
	if (!ret) {
		ERROR("TLS_reload failed\n");
		return true;
	}

	return true;

err:
	cJSON_Delete(root);

	return false;
}

static bool dbgStatus(uint8_t argc, char** argv)
{
	int     	ret;
	char    	errStr[256];
	char		ca_pem[2048];
	uint32_t	flags;

	mbedtls_x509_crt* cert;
	mbedtls_x509_crt* ca_cert;

	TLS_getCerts(&cert, &ca_cert);

	INFO("CA  : ");
	_print_cert_dates(ca_cert);
	INFO("cert: ");
	_print_cert_dates(cert);

	ret = mbedtls_x509_crt_verify(cert, ca_cert, NULL, NULL, &flags, NULL, NULL);

	if (ret) {
		char buf[256];
		mbedtls_x509_crt_verify_info(buf, sizeof(buf), "", flags);
		INFO("Certificate verification failed: %s\n", buf);
	} else {
		INFO("Certificate verification SUCCESS.\n");
	}

	return true;
}

static bool dbgVerify(uint8_t argc, char** argv)
{
	int     ret;
	mbedtls_x509_crt cert;
	mbedtls_x509_crt ca_chain;
	char	buf[2048];
	char*	ca_pem;
	uint32_t flags;

	mbedtls_x509_crt_init(&cert);
	mbedtls_x509_crt_init(&ca_chain);

	ret = FACTORY_get(factory_id_ca_pem, buf);
	if (!ret) {
		ERROR("CA not set\n");
		return true;
	}

	INFO("CA:\n%s\n", buf);
	if (mbedtls_x509_crt_parse(&ca_chain, (const unsigned char*)buf, strlen(buf) + 1) != 0) {
		PRINT("Failed to parse root CA cert\n");
		return true;
	}

	ret = CFG_get(cfg_id_inter_pem,  buf, sizeof(buf));
	if (!ret) {
		ERROR("intermediate certificate not present\n");
		return false;
	}
	INFO("INTERMEDIATE:\n%s\n", buf);
	ret = mbedtls_x509_crt_parse(&ca_chain, (unsigned char*)buf, strlen(buf) + 1);
	if (ret != 0) {
		ERROR("mbedtls_x509_crt_parse returned %d\n", ret);
		return true;
	}

	ret = CFG_get(cfg_id_cert_pem,  buf, sizeof(buf));
	if (!ret) {
		ERROR("device's certificate not present\n");
		return false;
	}
	INFO("CERT:\n%s\n", buf);
	if (mbedtls_x509_crt_parse(&cert, (const unsigned char*)buf, strlen(buf) + 1) != 0) {
		ERROR("mbedtls_x509_crt_parse returned %d\n", ret);
		return true;
	}

	ret = mbedtls_x509_crt_verify(&cert, &ca_chain, NULL, NULL, &flags, NULL, NULL);

	if (ret == 0) {
		PRINT("Certificate is valid and correctly signed.\n");
	} else {
		mbedtls_x509_crt_verify_info(buf, sizeof(buf), "", flags);
		PRINT("Certificate verification failed: %s\n", buf);
	}

	mbedtls_x509_crt_free(&cert);
	mbedtls_x509_crt_free(&ca_chain);

	return true;
}

static bool dbgCreateCsr(uint8_t argc, char** argv)
{
	bool    ret;
	char    csr_buf[2048];
	char    json[2048];
	mbedtls_pk_context*	pkey = TLS_getPkey();

	ret = _create_csr(pkey, (unsigned char*)csr_buf, sizeof(csr_buf));
	if (!ret) {
		ERROR("_create_csr failed\n");
		return true;
	}

	PRINT("csr:\n%s\n", csr_buf);

	_voultCreateCsrJson(csr_buf, "720h", json);

	PRINT("json:\n%s\n", json);

	return true;
}

static bool dbgRenew(uint8_t argc, char** argv)
{
	bool	ret;
	char	token[256];

	if (argc < 2) {
		ret = _vaultLogin(token);
		if (!ret) {
			PRINT("login failed\n");
			return true;
		}
	} else {
		strcpy(token, argv[2]);
	}

	PRINT("using token: %s\n", token);

	ret = _vaultRenew(token);
	if (!ret) {
		PRINT("reniew failed\n");
		return true;
	}

	return true;
}

static bool dbgLogin(uint8_t argc, char** argv)
{
	bool	ret;
	char	token[1024];

	ret = _vaultLogin(token);
	if (!ret) {
		PRINT("login failed\n");
		return true;
	}

	PRINT("TOKEN: %s\n", token);

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
	char*	http_result;

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

	resultSize = TLS_curl(url, method, header_key, header_value,  data, data_len, &http_result);

	PRINT_BUF("response",	PRINT_BUF_STYLE_ASC_HEX_SIZE_NL, http_result, resultSize);
	//PRINT("response:\n%s\n", http_client_result);

	return true;
}

// *INDENT-OFF*
DEBUG_MENU_START(g_menu)
	DEBUG_MENU_DIR("vault", NULL)
		DEBUG_MENU_CMD("status",    NULL,	NULL, dbgStatus)
		DEBUG_MENU_CMD("verify",    NULL,	NULL, dbgVerify)
		DEBUG_MENU_CMD("csr",       NULL,	NULL, dbgCreateCsr)
		DEBUG_MENU_CMD("curl",		NULL,	NULL, dbgCurl)
		DEBUG_MENU_CMD("renew",		NULL,	NULL, dbgRenew)
		DEBUG_MENU_CMD("login",		NULL,	NULL, dbgLogin)
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*

bool VAULT_init(void)
{
	DBG_TREE_add("/", g_menu);

	return true;
}

#endif
