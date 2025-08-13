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


#define VAULT_ROLE_NAME			"brain-space-all"
#define VAULT_URL_LOGIN			"/v1/auth/approle/login"
#define VAULT_URL_LOGIN_CERT	"/v1/auth/cert/login"
#define VAULT_URL_RENEW			"/v1/pki_int/sign/" VAULT_ROLE_NAME
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

	ret = CFG_get(cfg_id_sn, sn, sizeof(sn));
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
	TRACE_BUF("san_ext",	PRINT_BUF_STYLE_ASC_SIZE_NL, san_ext, san_length);

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
	pJson += sprintf(pJson, "\"", ttl);
	//pJson += sprintf(pJson, ",\"format\":\"pem_bundle\"", ttl);
	pJson += sprintf(pJson, ",\"ttl\":\"%s\"}", ttl);

	return true;
}

static void _print_cert_dates(const mbedtls_x509_crt* cert)
{
	tm_t	t;
	int32_t	from_days;
	int32_t	to_days;

	const mbedtls_x509_time* from = &cert->valid_from;
	const mbedtls_x509_time* to   = &cert->valid_to;

	t.year	= from->year;
	t.mon	= from->mon;
	t.day	= from->day;
	t.hour	= from->hour;
	t.min	= from->min;
	t.sec	= from->sec;

	from_days = TIME_mktime(&t) / 3600 / 24;

	t.year	= to->year;
	t.mon	= to->mon;
	t.day	= to->day;
	t.hour	= to->hour;
	t.min	= to->min;
	t.sec	= to->sec;

	to_days = TIME_mktime(&t) / 3600 / 24;

	PRINT("%04d-%02d-%02d %02d:%02d:%02d - ", from->year, from->mon, from->day, from->hour, from->min, from->sec);
	PRINT("%04d-%02d-%02d %02d:%02d:%02d ", to->year, to->mon, to->day, to->hour, to->min, to->sec);
	PRINT("%d - %d (%d)\n", from_days, to_days, to_days - from_days);
}

static void _printErrors(cJSON* root)
{
	cJSON* errors = cJSON_GetObjectItem(root, "errors");
	if (!errors) {
		return;
	}

	if (!cJSON_IsArray(errors)) {
		return;
	}

	int size = cJSON_GetArraySize(errors);
	for (int i = 0; i < size; i++) {
		cJSON* item = cJSON_GetArrayItem(errors, i);
		if (cJSON_IsString(item)) {
			ERROR(" <%s> ", item->valuestring);
		}
	}
}

#if 0
static bool _vaultLoginRoleSecret(char* o_pToken)
{
	bool	ret;
	char	url[128];
	char	data[1024];
	char	baseUrl[64];
	char	role[64];
	char	secret[64];
	char	t[256];
	int		http_result_size;
	char*	http_result = NULL;
	cJSON*	root = NULL;

	// TODO: fix size
	ret = CFG_get(cfg_id_vault_token, t, sizeof(t));
	if (ret) {
		if (t[0] != '\0') {
			INFO("skipping login. using token %s\n", t);
			strcpy(o_pToken, t);
			return true;
		}
	}

	ret = CFG_get(cfg_id_vault_url, baseUrl, sizeof(baseUrl));
	if (!ret) {
		ERROR("vault url not set\n");
		return false;
	}

	ret = CFG_get(cfg_id_vault_role, role, sizeof(role));
	if (!ret) {
		ERROR("vault role not set\n");
		return false;
	}

	ret = CFG_get(cfg_id_vault_secret, secret, sizeof(secret));
	if (!ret) {
		ERROR("vault secret not set\n");
		return false;
	}

	sprintf(url, "%s%s", baseUrl, VAULT_URL_LOGIN);
	sprintf(data, "{\"role_id\":\"%s\",\"secret_id\":\"%s\"}", role, secret);

	http_result = calloc(1, 2048);
	if (!http_result) {
		ERROR("failed to alloc http_result\n");
		return false;
	}

	http_result_size = TLS_curl(url, HTTP_METHOD_POST, NULL, NULL,  data, strlen(data), http_result, 2048);
	if (http_result_size < 0) {
		ERROR("TLS_curl failed\n");
		goto err;
	}

	root = cJSON_Parse(http_result);
	if (root == NULL) {
		ERROR("Failed to parse JSON\n");
		goto err;
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

	free(http_result);
	cJSON_Delete(root);
	return true;

err:
	//INFO_BUF("response",	PRINT_BUF_STYLE_ASC_HEX_SIZE_NL, http_result, http_result_size);

	if (root) {
		_printErrors(root);
		cJSON_Delete(root);
	}

	if (http_result) {
		free(http_result);
	}
	return false;
}
#endif

static bool _vaultLoginCert(char* o_pToken)
{
	bool	ret;
	char	url[128];
	char	baseUrl[64];
	int		http_result_size;
	char*	http_result = NULL;
	cJSON*	root = NULL;

	ret = CFG_get(cfg_id_vault_url, baseUrl, sizeof(baseUrl));
	if (!ret) {
		ERROR("vault url not set\n");
		return false;
	}

	sprintf(url, "%s%s", baseUrl, VAULT_URL_LOGIN_CERT);
	INFO("url: %s\n", url);

	http_result = calloc(1, 2048);
	if (!http_result) {
		ERROR("failed to alloc http_result\n");
		return false;
	}

	http_result_size = TLS_curl(url, HTTP_METHOD_POST, NULL, NULL,  NULL, 0, http_result, 2048);
	if (http_result_size < 0) {
		ERROR("TLS_curl failed\n");
		goto err;
	}

	root = cJSON_Parse(http_result);
	if (root == NULL) {
		ERROR("Failed to parse JSON\n");
		goto err;
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

	free(http_result);

	cJSON_Delete(root);
	return true;

err:
	//INFO_BUF("response",	PRINT_BUF_STYLE_ASC_SIZE_NL, http_result, http_result_size);
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

	if (root) {
		_printErrors(root);
		cJSON_Delete(root);
	}

	if (http_result) {
		free(http_result);
	}
	return false;
}

static bool _vaultRenew(char* token)
{
	bool    ret;
	esp_err_t err;
	int		resultSize;
	mbedtls_pk_context*	pkey = TLS_getPkey();
	char	baseUrl[64];
	char	url[256];

	char*	csr_buf		= NULL;
	char*	json		= NULL;
	char*	http_result = NULL;
	cJSON*	root		= NULL;
	char*	certStr		= NULL;

	ret = CFG_get(cfg_id_vault_url, baseUrl, sizeof(baseUrl));
	if (!ret) {
		ERROR("vault url not set\n");
		return false;
	}

	sprintf(url, "%s%s", baseUrl, VAULT_URL_RENEW);
	INFO("url: %s\n", url);

	csr_buf = calloc(1, 2048);
	ret = _create_csr(pkey, (unsigned char*)csr_buf, 2048);
	if (!ret) {
		ERROR("_create_csr failed\n");
		goto err;
	}

	INFO_BUF("CSR",	PRINT_BUF_STYLE_ASC_SIZE_NL, csr_buf, strlen(csr_buf));

	json = calloc(1, 2048);
	_voultCreateCsrJson(csr_buf, "720h", json);
	free(csr_buf);
	csr_buf = NULL;

	INFO("token: %s\n", token);
	INFO_BUF("JSON",	PRINT_BUF_STYLE_ASC_SIZE_NL, json, strlen(json));

	http_result	= calloc(1, 8192);
	if (!http_result) {
		ERROR("http_result alloc failed\n");
		return false;
	}

	resultSize = TLS_curl(url, HTTP_METHOD_POST, "X-Vault-Token", token, json, strlen(json), http_result, 8192);

	if (resultSize < 0) {
		ERROR("TLS_curl failed\n");
		goto err;
	}
	free(json);
	json = NULL;

	INFO_BUF("result",	PRINT_BUF_STYLE_ASC_SIZE_NL, http_result, resultSize);

	root = cJSON_Parse(http_result);
	if (root == NULL) {
		ERROR("Failed to parse JSON\n");
		goto err;
	}

	free(http_result);
	http_result = NULL;

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

	cJSON* ica = cJSON_GetObjectItem(data, "issuing_ca");
	if (ica) {
		INFO_BUF("ICA",	PRINT_BUF_STYLE_ASC_SIZE_NL | PRINT_BUF_STYLE_FORMAT_ASC, ica->valuestring, strlen(ica->valuestring));
	}
#if 0
	cJSON* chain = cJSON_GetObjectItem(data, "ca_chain");
	if (chain) {
		INFO("CHAIN\n");
		if (cJSON_IsArray(chain)) {
			int size = cJSON_GetArraySize(chain);
			INFO("CHAIN ARR size:%d\n", size);
			for (int i = 0; i < size; i++) {
				cJSON* item = cJSON_GetArrayItem(chain, i);
				if (cJSON_IsString(item)) {
					INFO("CHAIN %d: ", i);
					INFO_BUF("",	PRINT_BUF_STYLE_ASC_SIZE_NL | PRINT_BUF_STYLE_FORMAT_ASC, item->valuestring, strlen(item->valuestring));
				}
			}
			if (size >= 1) {
				cJSON* item = cJSON_GetArrayItem(chain, 0);
				if (cJSON_IsString(item)) {
					INFO("ICA:\n%s\n", item->valuestring);
					//ret = CFG_set(cfg_id_ica_pem, item->valuestring);
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
		//INFO_BUF("CHAIN",	PRINT_BUF_STYLE_ASC_SIZE_NL, chain->valuestring, strlen(chain->valuestring));
		//		INFO("CHAIN:\n%s\n", chain->valuestring);
	}
#endif

	cJSON* key = cJSON_GetObjectItem(data, "private_key");
	if (key) {
		INFO_BUF("PKEY",	PRINT_BUF_STYLE_ASC_SIZE_NL | PRINT_BUF_STYLE_FORMAT_ASC, key->valuestring, strlen(key->valuestring));
	}

	INFO_BUF("CERT",	PRINT_BUF_STYLE_ASC_SIZE_NL | PRINT_BUF_STYLE_FORMAT_ASC, cert->valuestring, strlen(cert->valuestring));

	certStr = calloc(1, 4096);
	if (!certStr) {
		ERROR("failed to allocate cert buffer\n");
		goto err;
	}

	snprintf(certStr, 4096, "%s\n%s", cert->valuestring, ica->valuestring);

	ret = CFG_set(cfg_id_cert_pem, certStr);
	if (!ret) {
		ERROR("NVS_set failed\n");
		goto err;
	}

	free(certStr);
	cJSON_Delete(root);

#if 0
	ret = TLS_reload();
	if (!ret) {
		ERROR("TLS_reload failed\n");
		return true;
	}
#endif

	return true;

err:
	if (root) {
		_printErrors(root);
		cJSON_Delete(root);
	}
	if (http_result) {
		free(http_result);
	}
	if (json) {
		free(json);
	}
	if (certStr) {
		free(certStr);
	}

	return false;
}

static bool _tlsVerify(void)
{
	int     	ret;
	char    	errStr[256];
	uint32_t	flags;

	mbedtls_x509_crt* cert;
	mbedtls_x509_crt* ca_cert;

	TLS_getCerts(&cert, &ca_cert);

	ret = mbedtls_x509_crt_verify(cert, ca_cert, NULL, NULL, &flags, NULL, NULL);

	if (ret) {
		char buf[256];
		mbedtls_x509_crt_verify_info(buf, sizeof(buf), "", flags);
		PRINT("TLS Certificate verification failed: %s\n", buf);
	} else {
		PRINT("TLS Certificate verification SUCCESS.\n");
	}

	PRINT("CA  : ");
	_print_cert_dates(ca_cert);
	PRINT("cert: ");
	_print_cert_dates(cert);

	return true;
}

static bool _certVerify(void)
{
	int     ret;
	mbedtls_x509_crt cert;
	mbedtls_x509_crt ca_chain;
	char	buf[4096];
	char*	ca_pem;
	uint32_t flags;

	mbedtls_x509_crt_init(&cert);
	mbedtls_x509_crt_init(&ca_chain);

	ret = CFG_get(cfg_id_ca_pem, buf, sizeof(buf));
	if (!ret) {
		ERROR("CA not set\n");
		return true;
	}

	//PRINT_BUF("CA",	PRINT_BUF_STYLE_ASC_SIZE_NL, buf, strlen(buf));

	if (mbedtls_x509_crt_parse(&ca_chain, (const unsigned char*)buf, strlen(buf) + 1) != 0) {
		PRINT("Failed to parse root CA cert\n");
		return true;
	}

	ret = CFG_get(cfg_id_cert_pem,  buf, sizeof(buf));
	if (!ret) {
		ERROR("device's certificate not present\n");
		return false;
	}

	//PRINT_BUF("CERT",	PRINT_BUF_STYLE_ASC_SIZE_NL, buf, strlen(buf));

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

static bool dbgVerify(uint8_t argc, char** argv)
{
	_certVerify();
	_tlsVerify();
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
	char	t[256];
	char*	token = NULL;

// *INDENT-OFF*
	ARGS_ENTRY_BEGIN(args)
		ARGS_ENTRY("t",		ARGS_TYPE_STRING,	0,	"role and secret login",	&token)
	ARGS_ENTRY_END()
// *INDENT-ON*

	ret = ARGS_readValues(argc, argv, args, NULL, NULL);
	if (!ret) {
		return false;
	}

	if (!token) {
		token = t;
		ret = _vaultLoginCert(token);
		if (!ret) {
			PRINT("login failed\n");
			return true;
		}
	}

	PRINT("using token: %s\n", token);

	ret = _vaultRenew(token);
	if (!ret) {
		PRINT("renew failed\n");
		return true;
	}

	return true;
}

static bool dbgLogin(uint8_t argc, char** argv)
{
	bool	ret;
	char	token[1024];
	bool	isRole = false;

// *INDENT-OFF*
	ARGS_ENTRY_BEGIN(args)
		ARGS_ENTRY("r",		ARGS_TYPE_SWITCH,	0,	"role and secret login",	&isRole)
	ARGS_ENTRY_END()
// *INDENT-ON*

	ret = ARGS_readValues(argc, argv, args, NULL, NULL);
	if (!ret) {
		return false;
	}

	if (isRole) {
		ERROR("not supported\n");
		return false;
		//ret = _vaultLoginRoleSecret(token);
	} else {
		ret = _vaultLoginCert(token);
	}

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

	http_result = malloc(8192);
	if (!http_result) {
		ERROR("http_result malloc failed\n");
		return true;
	}

	resultSize = TLS_curl(url, method, header_key, header_value,  data, data_len, http_result, 8192);
	if (resultSize > 0) {
		PRINT_BUF("response",	PRINT_BUF_STYLE_ASC_SIZE_NL, http_result, resultSize);
	}
	free(http_result);

	return true;
}

// *INDENT-OFF*
DEBUG_MENU_START(g_menu)
	DEBUG_MENU_DIR("vault", NULL)
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
