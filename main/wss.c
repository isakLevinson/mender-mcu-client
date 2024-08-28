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
#include "sdkconfig.h"
#include "cmd.h"

#define USE_SSL 1

#if !CONFIG_HTTPD_WS_SUPPORT
#error This example cannot be used unless HTTPD_WS_SUPPORT is enabled in esp-http-server component configuration
#endif

typedef struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
};

struct async_resp_arg events_async_resp = {0};

static const size_t max_clients = 4;


bool wss_send(struct async_resp_arg *i_pAsync, void* pBuf, size_t len)
{
    esp_err_t        ret;
    httpd_ws_frame_t pkt;
    struct async_resp_arg*  pAsync = i_pAsync;

    INFO_BUF("wss_send packet",	PRINT_BUF_STYLE_HEX_SIZE_NL, pBuf, len);

    if (!pAsync) {
        pAsync = &events_async_resp;
    }

    memset(&pkt, 0, sizeof(httpd_ws_frame_t));
    pkt.payload = (uint8_t*)pBuf;
    pkt.len = len;
    pkt.type = HTTPD_WS_TYPE_BINARY;

    ret = httpd_ws_send_frame_async(pAsync->hd, pAsync->fd, &pkt);
    if (ret != ESP_OK) {
        ERROR("httpd_ws_send_frame failed with %d\n", ret);
        return false;
    }

    return true;
}

bool _cmdSendResp(void* pArg, COMM_TYPE type, void* i_pBuf, uint16_t size)
{
    bool    ret;
    struct async_resp_arg *pAsync = (struct async_resp_arg*)pArg;

	uint8_t 	buf[300];
	uint8_t*	pBuf = buf;

    int s = *(int*)pArg;

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

static esp_err_t ws_handler(httpd_req_t *req)
{
    TRACE("ws_handler method=%d hd:0x%x fd:0x%x\n", req->method, req->handle, httpd_req_to_sockfd(req));

    if (req->method == HTTP_GET) {
        INFO("WS HTTP_GET Handshake done, the new connection was opened\n");
        return ESP_OK;
    }
    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

    // First receive the full ws message
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ERROR("httpd_ws_recv_frame failed to get frame len with %d\n", ret);
        return ret;
    }
    if (ws_pkt.len) {
        INFO("ws frame len is %d\n", ws_pkt.len);
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
    if (ws_pkt.type == HTTPD_WS_TYPE_PONG) {
        INFO("WS PONG message\n");
        free(buf);
        return wss_keep_alive_client_is_active(httpd_get_global_user_ctx(req->handle),
                httpd_req_to_sockfd(req));
        return 0;

    } else {
        if ((ws_pkt.type == HTTPD_WS_TYPE_TEXT) || (ws_pkt.type == HTTPD_WS_TYPE_BINARY)) {
            static uint8_t count;
            static char rsp[256];
            INFO("WS Received packet with message: type=%d\n", ws_pkt.type);
            INFO_BUF("WS Received packet",	PRINT_BUF_STYLE_HEX_SIZE_NL, ws_pkt.payload, ws_pkt.len);

            if (ws_pkt.len >= 3) {
                struct async_resp_arg async = {
                    .hd = req->handle,
                    .fd = httpd_req_to_sockfd(req),
                };

                CMD_CONTEXT context = {
                    .p_cbSend   = _cmdSendResp,
                    .pArg       = &async,
                };

                uint8_t len = ws_pkt.payload[0];
                uint8_t type = ws_pkt.payload[2];

                CMD_processMessage(&context, type, ws_pkt.payload+3, ws_pkt.len-3);
            }
        }

        if (ws_pkt.type == HTTPD_WS_TYPE_PING) {
            INFO("WS PING frame, Replying PONG\n");
            ws_pkt.type = HTTPD_WS_TYPE_PONG;
        } else if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
            ws_pkt.len = 0;
            ws_pkt.payload = NULL;
        }

        INFO("ws_handler: httpd_handle_t=%p, sockfd=%d, client_info:%d\n", req->handle,
                 httpd_req_to_sockfd(req), httpd_ws_get_fd_info(req->handle, httpd_req_to_sockfd(req)));
        free(buf);
        return ret;
    }
    free(buf);
    return ESP_OK;
}

static esp_err_t events_handler(httpd_req_t *req)
{
    TRACE("events_handler method=%d hd:0x%x fd:0x%x\n", req->method, req->handle, httpd_req_to_sockfd(req));

    if (req->method == HTTP_GET) {
        INFO("EVENTS HTTP_GET Handshake done, the new connection was opened\n");
        events_async_resp.hd  = req->handle;
        events_async_resp.fd  = httpd_req_to_sockfd(req);

        CMD_CONTEXT context = {
            .p_cbSend   = _cmdSendResp,
            .pArg       = &events_async_resp,
        };
        
        CMD_setStreamContext(&context);

        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

    // First receive the full ws message
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ERROR("httpd_ws_recv_frame failed to get frame len with %d\n", ret);
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
            ERROR("httpd_ws_recv_frame failed with %d\n", ret);
            free(buf);
            return ret;
        }
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_PONG) {
        INFO("Events PONG message\n");
        free(buf);
        return wss_keep_alive_client_is_active(httpd_get_global_user_ctx(req->handle), httpd_req_to_sockfd(req));

    }
    free(buf);
    return ESP_OK;
}

esp_err_t wss_open_fd(httpd_handle_t hd, int sockfd)
{
    INFO("wss_open hd:0x%x fd:0x%x\n", hd, sockfd);
    wss_keep_alive_t h = httpd_get_global_user_ctx(hd);
    return wss_keep_alive_add_client(h, sockfd);
}

void wss_close_fd(httpd_handle_t hd, int sockfd)
{
    if ((events_async_resp.hd == hd) && (events_async_resp.fd == sockfd)) {
        INFO("events_close_fd hd:0x%x fd:0x%x\n", hd, sockfd);
        memset(&events_async_resp, 0, sizeof(events_async_resp));
    } else {
        INFO("wss_close_fd hd:0x%x fd:0x%x\n", hd, sockfd);
    }


    wss_keep_alive_t h = httpd_get_global_user_ctx(hd);
    wss_keep_alive_remove_client(h, sockfd);
    close(sockfd);
}

static void send_ping(void *arg)
{
    struct async_resp_arg *resp_arg = arg;
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

bool client_not_alive_cb(wss_keep_alive_t h, int fd)
{
    ERROR("client_not_alive_cb() closing fd %d\n", fd);
    httpd_sess_trigger_close(wss_keep_alive_get_user_ctx(h), fd);
    return true;
}

bool check_client_alive_cb(wss_keep_alive_t h, int fd)
{
    TRACE("check_client_alive_cb() Checking if client (fd=%d) is alive\n", fd);
    struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
    resp_arg->hd = wss_keep_alive_get_user_ctx(h);
    resp_arg->fd = fd;

    if (httpd_queue_work(resp_arg->hd, send_ping, resp_arg) == ESP_OK) {
        return true;
    }
    return false;
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

httpd_handle_t wss_start_server(void)
{
    // Start the httpd server
    httpd_handle_t server = NULL;
    INFO("Starting server");

    // Create and initialize the keep-alive configuration
    wss_keep_alive_config_t keep_alive_config = KEEP_ALIVE_CONFIG_DEFAULT();
    keep_alive_config.max_clients = max_clients;
    keep_alive_config.client_not_alive_cb = client_not_alive_cb;
    keep_alive_config.check_client_alive_cb = check_client_alive_cb;

    // Start the keep-alive engine
    wss_keep_alive_t keep_alive = wss_keep_alive_start(&keep_alive_config);

#if USE_SSL
    httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();

    conf.httpd.max_open_sockets = max_clients;
    conf.httpd.global_user_ctx = keep_alive;
    conf.httpd.open_fn = wss_open_fd;
    conf.httpd.close_fn = wss_close_fd;

    // Configure server certificate and private key
    extern const unsigned char servercert_start[] asm("_binary_servercert_pem_start");
    extern const unsigned char servercert_end[]   asm("_binary_servercert_pem_end");
    conf.servercert = servercert_start;
    conf.servercert_len = servercert_end - servercert_start;

    extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
    extern const unsigned char prvtkey_pem_end[]   asm("_binary_prvtkey_pem_end");
    conf.prvtkey_pem = prvtkey_pem_start;
    conf.prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;

    esp_err_t ret = httpd_ssl_start(&server, &conf);
    if (ESP_OK != ret) {
        ERROR("Error starting server!\n");
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
    httpd_register_uri_handler(server, &uri_ws);
    httpd_register_uri_handler(server, &uri_events);
    wss_keep_alive_set_user_ctx(keep_alive, server);

    return server;
}

static esp_err_t stop_wss_echo_server(httpd_handle_t server)
{
    // Stop the keep-alive engine
    wss_keep_alive_stop(httpd_get_global_user_ctx(server));

    // Stop the HTTP server with SSL
    return httpd_ssl_stop(server);
}

static void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        // Stop the server if it is running
        if (stop_wss_echo_server(*server) == ESP_OK) {
            *server = NULL;
        } else {
            ERROR("Failed to stop https server");
        }
    }
}

#if 0
static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        // Start the server if it is not running
        *server = start_wss_echo_server();
    }
}
#endif
