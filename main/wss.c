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

struct async_resp_arg events_async_resp;

static const size_t max_clients = 4;

static struct {
	httpd_handle_t handle;
	bool			ota_new_restart;
	struct {
		int	counter;
		struct {
			int	fd;
			char* type;
		} sockets[MAX_SOCKETS_COUNT];
	} dbg;
} g_server = {0};

static bool _socketAdd(int fd)
{
	uint8_t	i;
	for (i=0; i<MAX_SOCKETS_COUNT; i++) {
		if (!g_server.dbg.sockets[i].fd) {
			g_server.dbg.sockets[i].fd = fd;
			return true;
		}
	}
	ERROR("acn't add new socket %d\n", fd);

	return false;
}

static bool _socketDel(int fd)
{
	uint8_t	i;
	for (i=0; i<MAX_SOCKETS_COUNT; i++) {
		if (g_server.dbg.sockets[i].fd == fd) {
			g_server.dbg.sockets[i].fd = 0;
			return true;
		}
	}

	ERROR("trying to close unexsisting socket %d\n", fd);
	return false;
}

static bool _socketSetType(int fd, char* type)
{
	uint8_t	i;
	for (i=0; i<MAX_SOCKETS_COUNT; i++) {
		if (g_server.dbg.sockets[i].fd == fd) {
			g_server.dbg.sockets[i].type = type;
			return true;
		}
	}

	ERROR("trying to set type of an unexsisting socket %d\n", fd);

	return false;
}


static void send_ping(void* arg)
{
	struct async_resp_arg* resp_arg = arg;
	httpd_handle_t hd = resp_arg->hd;
	int fd = resp_arg->fd;
	httpd_ws_frame_t ws_pkt;
	memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
	ws_pkt.payload = NULL;
	ws_pkt.len = 0;
	ws_pkt.type = HTTPD_WS_TYPE_PING;

	httpd_ws_send_frame_async(hd, fd, &ws_pkt);
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

static portMUX_TYPE my_spinlock = portMUX_INITIALIZER_UNLOCKED;

bool send_binary(httpd_handle_t hd, int fd, void* pBuf, size_t size)
{
	int status;
	TRACE("send_binary fd:%d\n", fd);

	httpd_ws_frame_t ws_pkt;
	memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
	ws_pkt.payload = pBuf;
	ws_pkt.len = size;
	ws_pkt.type = HTTPD_WS_TYPE_BINARY;

	status = httpd_ws_send_frame_async(hd, fd, &ws_pkt);

	if (ESP_OK != status) {
		ERROR("send_binary: httpd_ws_send_frame_async %d\n", status);
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
static esp_err_t ws_handler(httpd_req_t* req)
{
	int fd = httpd_req_to_sockfd(req);
	TRACE("ws_handler method=%d hd:0x%x fd:%d\n", req->method, req->handle, fd);

	//mbedtls_ssl_context *ssl_ctx = httpd_ssl_get_ssl_ctx(req);
	httpd_resp_set_hdr(req, "Connection", "keep-alive");

	if (req->method == HTTP_GET) {
		INFO("WS HTTP_GET Handshake done, the new connection was opened\n");
		_socketSetType(fd, "WS");
		return ESP_OK;
	}
	httpd_ws_frame_t ws_pkt;
	uint8_t* buf = NULL;
	memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

	// First receive the full ws message
	esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
	if (ret != ESP_OK) {
		ERROR("httpd_ws_recv_frame ws failed to get frame len with %d\n", ret);
		return ret;
	}

	if (ws_pkt.len) {
		TRACE("WS frame len is %d\n", ws_pkt.len);
		buf = calloc(1, ws_pkt.len + 1);
		if (buf == NULL) {
			ERROR("Failed to calloc memory for buf\n");
			return ESP_ERR_NO_MEM;
		}
		ws_pkt.payload = buf;
		ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
		if (ret != ESP_OK) {
			ERROR("httpd_ws_recv_frame failed with %d\n", ret);
			free(buf);
			return ret;
		}
	}

	switch (ws_pkt.type) {
		case HTTPD_WS_TYPE_PING:
			INFO("WS PING frame, Replying with PONG\n");
			ws_pkt.type = HTTPD_WS_TYPE_PONG;
			break;

		case HTTPD_WS_TYPE_PONG:
			INFO("WS PONG message h:%x\n", req->handle);
			free(buf);
			return wss_keep_alive_client_is_active(httpd_get_global_user_ctx(req->handle), fd);
			break;

		case HTTPD_WS_TYPE_TEXT:
			INFO("HTTPD_WS_TYPE_TEXT len:%d\n", ws_pkt.len);
			break;

		case HTTPD_WS_TYPE_BINARY: {
				struct async_resp_arg async = {
					.hd = req->handle,
					.fd = fd,
				};

				CMD_CONTEXT context = {
					.p_cbSend   = _cmdSendResp,
					.pArg       = &async,
				};

				if (ws_pkt.len < 3) {
					WARN("HTTPD_WS_TYPE_BINARY short incoming message. ignoring len:%s\n", ws_pkt.len);
				}

				uint8_t len = ws_pkt.payload[0];
				uint8_t type = ws_pkt.payload[2];
				INFO("HTTPD_WS_TYPE_BINARY len:%d\n", ws_pkt.len);
				INFO("WS Received packet with message: type=%d len=%d cmd:(t:%d, l:%d)\n", ws_pkt.type, ws_pkt.len, type, len);
				CMD_processMessage(&context, type, ws_pkt.payload + 3, ws_pkt.len - 3);
			}
			break;

		case HTTPD_WS_TYPE_CLOSE:
			INFO("CLOSE\n");
			ws_pkt.len = 0;
			ws_pkt.payload = NULL;
			break;

		case HTTPD_WS_TYPE_CONTINUE:
			INFO("CONTINUE\n");
			break;
	}

	TRACE("ws_handler: httpd_handle_t=%p, fd=%d, client_info:%d\n",
	    req->handle,
	    httpd_req_to_sockfd(req),
	    httpd_ws_get_fd_info(req->handle, fd));

	free(buf);
	return ESP_OK;
}

static esp_err_t events_handler(httpd_req_t* req)
{
	esp_err_t ret;
	httpd_ws_frame_t ws_pkt;
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

	memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

	// First receive the full ws message
	ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
	if (ret != ESP_OK) {
		ERROR("httpd_ws_recv_frame evt failed to get frame len with %d\n", ret);
		return ret;
	}
	if (ws_pkt.len) {
		INFO("ev events frame len is %d\n", ws_pkt.len);
		buf = calloc(1, ws_pkt.len + 1);
		if (buf == NULL) {
			ERROR("Failed to calloc memory for buf\n");
			return ESP_ERR_NO_MEM;
		}
		ws_pkt.payload = buf;
		ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
		if (ret != ESP_OK) {
			ERROR("events httpd_ws_recv_frame %d\n", ret);
			free(buf);
			return ret;
		}
	}

	if (ws_pkt.type == HTTPD_WS_TYPE_PONG) {
		INFO("Events PONG message\n");
		free(buf);
		return wss_keep_alive_client_is_active(httpd_get_global_user_ctx(req->handle), fd);

	}
	free(buf);

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

	return wss_keep_alive_add_client(h, fd);
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

	esp_err_t ret = httpd_start(&server, &conf);
	if (ESP_OK != ret) {
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
