#define DEF_DBG_MODULE	DBG_MODULE_WSS

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include <esp_event.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_wifi.h"
#include "lwip/sockets.h"
#include <esp_https_server.h>
#include "wss_keepalive.h"
#include "wss.h"
#include "cmd.h"
#include "wifi.h"
#include "nvs.h"
#include "mdns.h"
#include "config.h"

#define USE_SSL 1

#if !CONFIG_HTTPD_WS_SUPPORT
#error This example cannot be used unless HTTPD_WS_SUPPORT is enabled in esp-http-server component configuration
#endif

#define MAX_SOCKETS_COUNT	8

struct send_arg_t {
	httpd_handle_t  	hd;
	int             	fd;
	httpd_ws_frame_t	pkt;
	size_t          	size;
	int					counter;
	uint8_t         	buf[];
};

struct socket_desc_t {
	int					fd;
	char*				type;
	SemaphoreHandle_t	txMutex;
	StaticSemaphore_t	txMutexBuffer;
};

struct async_resp_arg events_async_resp;

static const size_t max_clients = 4;

static struct {
	httpd_handle_t 		handle;
	bool				ota_new_restart;

	struct {
		int	counter;
		struct socket_desc_t sockets[MAX_SOCKETS_COUNT];
	} dbg;
} g_server = {0};

static struct socket_desc_t* _socketGet(int fd)
{
	uint8_t	i;
	for (i=0; i<MAX_SOCKETS_COUNT; i++) {
		if (g_server.dbg.sockets[i].fd == fd) {
			return &g_server.dbg.sockets[i];
		}
	}

	return NULL;
}

static void _socketPrint(char* prefix, int fd)
{
	struct socket_desc_t*	sock = _socketGet(fd);
	if (!sock) {
		WARN("%s unexpected socket %d\n", prefix, fd);
		return;
	}

	if (!sock->type) {
		WARN("%s unknown socket type %d\n", prefix, fd);
		return;
	}

	INFO("%s %d %s\n", prefix, fd, sock->type);
}

static bool _socketAdd(int fd)
{
	uint8_t	i;
	for (i=0; i<MAX_SOCKETS_COUNT; i++) {
		if (!g_server.dbg.sockets[i].fd) {
			g_server.dbg.sockets[i].fd = fd;
			g_server.dbg.sockets[i].txMutex = xSemaphoreCreateMutexStatic(&g_server.dbg.sockets[i].txMutexBuffer);
			return true;
		}
	}
	ERROR("can't add new socket %d\n", fd);

	return false;
}

static bool _socketDel(int fd)
{
	struct socket_desc_t* sock = _socketGet(fd);

	if (!sock) {
		ERROR("trying to close unexsisting socket %d\n", fd);
		return false;
	}

	vSemaphoreDelete(sock->txMutex);

	if (sock->type) {
		INFO("closed %d %s\n", fd, sock->type);
	} else {
		INFO("closed %d UNKNOWN\n", fd);
	}

	sock->fd = 0;

	return true;
}

static bool _socketSetType(int fd, char* type)
{
	struct socket_desc_t* sock = _socketGet(fd);

	if (!sock) {
		ERROR("trying to set type of an unexsisting socket %d\n", fd);
		return false;
	}

	INFO("set socket type %d %s\n", fd, type);
	sock->type = type;

	return true;
}

static bool _tx(httpd_handle_t hd, int fd, httpd_ws_frame_t* pkt)
{
	esp_err_t	status;
	struct socket_desc_t*	sock = _socketGet(fd);
	if (!sock) {
		return false;
	}

	xSemaphoreTake(sock->txMutex, portMAX_DELAY);

	status = httpd_ws_send_frame_async(hd, fd, pkt);
	if (ESP_OK != status) {
		ERROR("send_binary: httpd_ws_send_frame_async %d\n", status);
		return false;
	}

	xSemaphoreGive(sock->txMutex);

	return true;
}

static void send_ping(void* arg)
{
	struct async_resp_arg* resp_arg = arg;
	httpd_handle_t hd = resp_arg->hd;
	int fd = resp_arg->fd;
	httpd_ws_frame_t pkt;
	memset(&pkt, 0, sizeof(httpd_ws_frame_t));
	pkt.payload = NULL;
	pkt.len = 0;
	pkt.type = HTTPD_WS_TYPE_PING;

	_tx(hd, fd, &pkt);
	free(resp_arg);
}

bool check_client_alive_cb(wss_keep_alive_t h, int fd)
{
	int status;
	TRACE("check_client_alive_cb() Checking if client (fd=%d) is alive\n", fd);
#if 0
	struct async_resp_arg* resp_arg = malloc(sizeof(struct async_resp_arg));
	resp_arg->hd = wss_keep_alive_get_user_ctx(h);
	resp_arg->fd = fd;
	status = httpd_queue_work(resp_arg->hd, send_ping, resp_arg);
	if (ESP_OK != status) {
		ERROR("check_client_alive_cb: failed to send ping\n");
		free(resp_arg);
		return false;
	}
#endif
	return true;
}

bool send_binary(httpd_handle_t hd, int fd, void* pBuf, size_t size)
{
	bool ret;
	TRACE("send_binary fd:%d\n", fd);

	httpd_ws_frame_t pkt;
	memset(&pkt, 0, sizeof(httpd_ws_frame_t));
	pkt.payload = pBuf;
	pkt.len = size;
	pkt.type = HTTPD_WS_TYPE_BINARY;

	ret = _tx(hd, fd, &pkt);

	if (!ret) {
		return false;
	} else {
		g_server.dbg.counter++;
	}
	return true;
}

bool wss_send(struct async_resp_arg* i_pAsync, void* pBuf, size_t len)
{
	bool        ret;
	struct async_resp_arg*  pAsync = i_pAsync;

	TRACE_BUF("wss_send packet",	PRINT_BUF_STYLE_HEX_SIZE_NL, pBuf, len);

	if (!pAsync) {
		pAsync = &events_async_resp;
	}

	ret = send_binary(pAsync->hd, pAsync->fd, pBuf, len);

	if (!ret) {
		ERROR("httpd_ws_send_frame\n");
		return false;
	}

	return true;
}

bool _cmdSendResp(void* pArg, uint8_t type, void* i_pBuf, uint16_t size)
{
	bool    ret;
	struct async_resp_arg* pAsync = (struct async_resp_arg*)pArg;

	uint8_t 	buf[1600];
	uint8_t*	pBuf = buf;

	*(uint16_t*)pBuf	= size;
	pBuf += 2;
	*pBuf	= type;
	pBuf++;

	memcpy(pBuf, i_pBuf, size);
	pBuf += size;

	TRACE_BUF("wss_cmdSendResp", PRINT_BUF_STYLE_HEX_SIZE_NL, buf, pBuf - buf);

	ret = wss_send(pAsync, buf, pBuf - buf);

	return ret;
}

static bool common_handler(httpd_req_t* req, httpd_ws_frame_t* pkt)
{
	esp_err_t		ret;
	httpd_resp_set_hdr(req, "Connection", "keep-alive");
	memset(pkt, 0, sizeof(httpd_ws_frame_t));
	int fd = httpd_req_to_sockfd(req);
	struct socket_desc_t* desc = _socketGet(fd);
	char* fdType = "";

	if (desc) {
		fdType = desc->type;
	}

	TRACE("common_handler %d (%s)\n", fd, fdType);

	// First receive the full ws message
	ret = httpd_ws_recv_frame(req, pkt, 0);
	if (ret != ESP_OK) {
		ERROR("httpd_ws_recv_frame ws failed to get frame len with %d\n", ret);
		return false;
	}

	if (pkt->len) {
		TRACE("frame len is %d\n", pkt->len);
		pkt->payload = calloc(1, pkt->len + 1);
		if (!pkt->payload) {
			ERROR("Failed to calloc memory for ws_pkt.payload\n");
			return false;
		}

		ret = httpd_ws_recv_frame(req, pkt, pkt->len);
		if (ret != ESP_OK) {
			ERROR("httpd_ws_recv_frame failed with %d\n", ret);
			free(pkt->payload);
			return false;
		}
	}

	switch (pkt->type) {
		case HTTPD_WS_TYPE_PING:
			INFO("PING frame, Replying with PONG fd:%d (%s)\n", fd, fdType);
			pkt->type = HTTPD_WS_TYPE_PONG;
			ret = _tx(req->handle, fd, pkt);
			break;

		case HTTPD_WS_TYPE_PONG:
			INFO("PONG message h:%x, fd:%d\n", req->handle, fd);
			free(pkt->payload);
			pkt->payload = NULL;
			return wss_keep_alive_client_is_active(httpd_get_global_user_ctx(req->handle), fd);
			break;

		case HTTPD_WS_TYPE_TEXT:
			INFO("HTTPD_WS_TYPE_TEXT len:%d\n", pkt->len);
			break;

		case HTTPD_WS_TYPE_CLOSE:
			INFO("CLOSE fd:%d\n", fd);
			pkt->len = 0;
			free(pkt->payload);
			pkt->payload = NULL;
			ret = _tx(req->handle, fd, pkt);
			break;

		case HTTPD_WS_TYPE_CONTINUE:
			INFO("CONTINUE fd:%d\n", fd);
			break;

		default:
	}

	return true;
}

static esp_err_t ws_handler(httpd_req_t* req)
{
	httpd_ws_frame_t pkt;

	int fd = httpd_req_to_sockfd(req);
	TRACE("ws_handler method=%d hd:0x%x fd:%d\n", req->method, req->handle, fd);

	if (req->method == HTTP_GET) {
		INFO("WS HTTP_GET Handshake done, the new connection was opened\n");
		_socketSetType(fd, "WS");
		return ESP_OK;
	}

	common_handler(req, &pkt);

	if (HTTPD_WS_TYPE_BINARY == pkt.type) {
		struct async_resp_arg async = {
			.hd = req->handle,
			.fd = fd,
		};

		CMD_CONTEXT context = {
			.p_cbSend   = _cmdSendResp,
			.pArg       = &async,
		};

		if (pkt.len < 3) {
			WARN("HTTPD_WS_TYPE_BINARY short incoming message. ignoring len:%s\n", pkt.len);
		}

		uint8_t len = pkt.payload[0];
		uint8_t type = pkt.payload[2];
		INFO("HTTPD_WS_TYPE_BINARY len:%d\n", pkt.len);
		INFO("WS Received packet with message: type=%d len=%d cmd:(t:%d, l:%d)\n", pkt.type, pkt.len, type, len);
		CMD_processMessage(&context, type, pkt.payload + 3, pkt.len - 3);
	}

	TRACE("ws_handler: httpd_handle_t=%p, fd=%d, client_info:%d\n",
	    req->handle,
	    httpd_req_to_sockfd(req),
	    httpd_ws_get_fd_info(req->handle, fd));

	free(pkt.payload);
	pkt.payload = NULL;
	return ESP_OK;
}

static esp_err_t events_handler(httpd_req_t* req)
{
	esp_err_t ret;
	httpd_ws_frame_t pkt;
	uint8_t* buf = NULL;

	int fd = httpd_req_to_sockfd(req);

	TRACE("events_handler method=%d hd:0x%x fd:%d\n", req->method, req->handle, fd);

	if (req->method == HTTP_GET) {
		INFO("EVENTS HTTP_GET\n");
		events_async_resp.hd  = req->handle;
		events_async_resp.fd  = fd;
		_socketSetType(fd, "EVT");

		CMD_CONTEXT context = {
			.p_cbSend   = _cmdSendResp,
			.pArg       = &events_async_resp,
		};

		CMD_setStreamContext(&context);

		if (g_server.ota_new_restart) {
			INFO("OTA was recently performed. Sending new version notification\n");
			g_server.ota_new_restart = false;
			CMD_sendOtaStatusEvent();
			NVS_set(nvs_id_ota_updated,  "0");
		}

		return ESP_OK;
	}

	common_handler(req, &pkt);

	free(pkt.payload);
	pkt.payload = NULL;

	return ESP_OK;
}

static esp_err_t _config_handler(httpd_req_t* req)
{
	//esp_err_t ret;
	bool    	ret;
	char    	buf[256];
	bool    	validSsid;
	bool    	validPasswd;
	const char* pResp = "OK\n";

	int fd = httpd_req_to_sockfd(req);

	INFO("config_handler method=%d hd:0x%x fd:%d\n", req->method, req->handle, fd);

	if (req->method != HTTP_POST) {
		WARN("unsupported method %s. must be POST\n", req->method);
		_socketSetType(fd, "CFG");

		return ESP_OK;
	}

	ret = httpd_req_recv(req, buf, req->content_len);
	if (!ret) {
		ERROR("httpd_req_recv failed\n");
		return ESP_FAIL;
	}

	//INFO("POST: %.*s\n", ret, buf);
	INFO_BUF("/config POST",	PRINT_BUF_STYLE_ASC_SIZE_NL, buf, req->content_len);
	ret = CFG_parseWssCommand(buf, req->content_len);
	if (!ret) {
		pResp = "ERROR\n";
	}

	/* Send response with body set as the
	 * string passed in user context*/
	//const char* resp_str = (const char*) req->user_ctx;
	//httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
	httpd_resp_send(req, pResp, HTTPD_RESP_USE_STRLEN);

	// End response
	httpd_resp_send_chunk(req, NULL, 0);

	return ESP_OK;
}

static esp_err_t rest_handler(httpd_req_t* req)
{
	bool    ret;
	char    buf[256];

	int fd = httpd_req_to_sockfd(req);

	INFO("rest_handler method=%d hd:0x%x fd:%d\n", req->method, req->handle, fd);

	if (req->method != HTTP_POST) {
		WARN("unsupported method %s. must be POST\n", req->method);
		_socketSetType(fd, "REST");
		return ESP_OK;
	}

	ret = httpd_req_recv(req, buf, req->content_len);
	if (!ret) {
		ERROR("httpd_req_recv failed\n");
		return ESP_FAIL;
	}

	INFO_BUF("/rest POST",	PRINT_BUF_STYLE_ASC_SIZE_NL, buf, req->content_len);

	httpd_resp_send(req, "OK\n", HTTPD_RESP_USE_STRLEN);
	//	httpd_resp_send_chunk(req, NULL, 0);

	return ESP_OK;
}

esp_err_t wss_open_fd(httpd_handle_t hd, int fd)
{
	INFO("wss_open hd:0x%x fd:%d\n", hd, fd);

	wss_keep_alive_t h = httpd_get_global_user_ctx(hd);

	_socketAdd(fd);

	return ESP_OK;
//	return wss_keep_alive_add_client(h, fd);
}

void wss_close_fd(httpd_handle_t hd, int fd)
{
	if ((events_async_resp.hd == hd) && (events_async_resp.fd == fd)) {
		INFO("events_close_fd hd:0x%x fd:%d\n", hd, fd);
		memset(&events_async_resp, 0, sizeof(events_async_resp));
	} else {
		INFO("wss_close_fd hd:0x%x fd:%d\n", hd, fd);
	}

	wss_keep_alive_t h = httpd_get_global_user_ctx(hd);
	wss_keep_alive_remove_client(h, fd);
	close(fd);
	_socketDel(fd);
}

bool client_not_alive_cb(wss_keep_alive_t h, int fd)
{
	ERROR("client_not_alive_cb() closing fd %d\n", fd);
	httpd_sess_trigger_close(wss_keep_alive_get_user_ctx(h), fd);
	return true;
}

static const httpd_uri_t uri_ws = {
	.uri        = "/ws",
	.method     = HTTP_GET,
	.handler    = ws_handler,
	.user_ctx   = NULL,
	.is_websocket = true,
	.handle_ws_control_frames = true
};

static const httpd_uri_t uri_events = {
	.uri        = "/events",
	.method     = HTTP_GET,
	.handler    = events_handler,
	.user_ctx   = NULL,
	.is_websocket = true,
	.handle_ws_control_frames = true
};

static const httpd_uri_t uri_rest = {
	.uri        = "/rest",
	.method     = HTTP_POST,
	.handler    = rest_handler,
	.user_ctx   = NULL,
	.is_websocket = false,
	.handle_ws_control_frames = true
};

bool wss_config_start(void)
{
	static const httpd_uri_t uri_config = {
		.uri        = "/config",
		.method     = HTTP_POST,
		.handler    = _config_handler,
		.user_ctx   = NULL,
		.is_websocket = true,
		.handle_ws_control_frames = true
	};

	INFO("wss_config_start\n");
	httpd_register_uri_handler(g_server.handle, &uri_config);

	return true;
}

bool wss_config_stop(void)
{
	httpd_unregister_uri(g_server.handle, "/config");

	return true;
}

static bool dbgStatus(uint8_t argc, char** argv)
{

	uint8_t	i;
	for (i=0; i<MAX_SOCKETS_COUNT; i++) {
		if (g_server.dbg.sockets[i].fd) {
			PRINT("%d ", g_server.dbg.sockets[i].fd);
			if (g_server.dbg.sockets[i].type) {
				PRINT("%s ", g_server.dbg.sockets[i].type);
			}
			PRINT("\n");
		}
	}
	PRINT("\n");

	return true;
}

// *INDENT-OFF*
DEBUG_MENU_START(g_menu)
	DEBUG_MENU_DIR("wss", NULL)
		DEBUG_MENU_CMD("status",	        NULL,		NULL, dbgStatus)
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*

bool wss_init(void)
{
	bool		ret;
	esp_err_t	err;
	char		buf[32];

	if (g_server.handle) {
		WARN("wss already started\n");
		return false;
	}

	DBG_TREE_add("/", g_menu);

	// Start the httpd server
	INFO("Starting server");

	ret = NVS_get(nvs_id_ota_updated,  buf);
	if (ret) {
		if (!strcmp(buf, "1")) {
			g_server.ota_new_restart = true;
		}
	}

	// Create and initialize the keep-alive configuration
	wss_keep_alive_config_t keep_alive_config = KEEP_ALIVE_CONFIG_DEFAULT();
	keep_alive_config.max_clients = max_clients;
	keep_alive_config.client_not_alive_cb = client_not_alive_cb;
	keep_alive_config.check_client_alive_cb = check_client_alive_cb;

	// Start the keep-alive engine
	wss_keep_alive_t keep_alive = wss_keep_alive_start(&keep_alive_config);
	wss_keep_alive_set_user_ctx(keep_alive, g_server.handle);

#if USE_SSL
	httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();

	conf.httpd.global_user_ctx = keep_alive;
	conf.httpd.max_open_sockets = max_clients;
	conf.httpd.open_fn = wss_open_fd;
	conf.httpd.close_fn = wss_close_fd;

	// Configure server certificate and private key
	extern const unsigned char server_cert_start[] asm("_binary_server_crt_start");
	extern const unsigned char server_cert_end[]   asm("_binary_server_crt_end");
	conf.servercert = server_cert_start;
	conf.servercert_len = server_cert_end - server_cert_start;

	extern const unsigned char prvtkey_pem_start[] asm("_binary_server_key_start");
	extern const unsigned char prvtkey_pem_end[]   asm("_binary_server_key_end");
	conf.prvtkey_pem = prvtkey_pem_start;
	conf.prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;

#if 0
	extern const unsigned char ca_cert_start[] asm("_binary_ca_crt_start");
	extern const unsigned char ca_cert_end[]   asm("_binary_ca_crt_end");
	conf.cacert_pem = ca_cert_start;
	conf.cacert_len = ca_cert_end - ca_cert_start;
#endif

	conf.httpd.keep_alive_enable = false;
	conf.session_tickets = true;

	err = httpd_ssl_start(&g_server.handle, &conf);
	if (ESP_OK != err) {
		ERROR("Error starting server %d\n", err);
		return NULL;
	}

#else
	httpd_config_t conf = HTTPD_DEFAULT_CONFIG();

	conf.open_fn = wss_open_fd;
	conf.close_fn = wss_close_fd;

	err = httpd_start(&g_server.handle, &conf);
	if (ESP_OK != err) {
		ERROR("Error starting server!\n");
		return NULL;
	}
#endif

	// Set URI handlers
	INFO("Registering URI handlers");
	httpd_register_uri_handler(g_server.handle, &uri_ws);
	httpd_register_uri_handler(g_server.handle, &uri_events);
	httpd_register_uri_handler(g_server.handle, &uri_rest);

	return true;
}
