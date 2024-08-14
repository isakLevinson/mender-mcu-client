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

#if !CONFIG_HTTPD_WS_SUPPORT
#error This example cannot be used unless HTTPD_WS_SUPPORT is enabled in esp-http-server component configuration
#endif

struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
};

static const size_t max_clients = 4;


esp_err_t wss_send(httpd_req_t* pReq, void* pBuf, size_t len)
{
    esp_err_t        ret;
    httpd_ws_frame_t pkt;

    INFO("wss_send\n");

    memset(&pkt, 0, sizeof(httpd_ws_frame_t));
    pkt.payload = (uint8_t*)pBuf;
    pkt.len = len;
    pkt.type = HTTPD_WS_TYPE_TEXT;

    // Send response message
        // Get the current time
    TickType_t startTime = xTaskGetTickCount();
    int sent_count = 0;
    // Loop for 10 seconds
    INFO("start sending data at %lu:\n", startTime);
    while ((xTaskGetTickCount() - startTime) < (10 * configTICK_RATE_HZ)) {
// Create a response structure
        ret = httpd_ws_send_frame(pReq, &pkt);
        if (ret != ESP_OK) {
            ERROR("httpd_ws_send_frame failed with %d\n", ret);
        }
        sent_count += pkt.len;
        // INFO("sent data at %lu:", xTaskGetTickCount());
    }
    INFO("end sending data at %lu:\n", xTaskGetTickCount());
    INFO("sent %d bytes\n", sent_count);

    return 0;
}

bool _cmdSendResp(void* pArg, COMM_TYPE Message_Type, void* i_pBuf, uint16_t size)
{
    httpd_req_t *req = (httpd_req_t*)pArg;

    INFO("_cmdSendResp\n");

    wss_send(req, i_pBuf, size);

    return true;
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    INFO("ws_handler method=%d\n", req->method);

    if (req->method == HTTP_GET) {
        INFO("HTTP_GET Handshake done, the new connection was opened\n");
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
    INFO("frame len is %d\n", ws_pkt.len);
    if (ws_pkt.len) {
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
        INFO("Received PONG message\n");
        free(buf);
        return wss_keep_alive_client_is_active(httpd_get_global_user_ctx(req->handle),
                httpd_req_to_sockfd(req));
        return 0;

    } else if (ws_pkt.type == HTTPD_WS_TYPE_TEXT || ws_pkt.type == HTTPD_WS_TYPE_PING || ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
            static uint8_t count;
            static char rsp[256];
            //INFO("Received packet with message: <%s>\n", ws_pkt.payload);
            INFO_BUF("Received packet",	PRINT_BUF_STYLE_HEX_SIZE_NL, ws_pkt.payload, ws_pkt.len);

            if (ws_pkt.len >= 2) {
                CMD_CONTEXT context = {
                    .p_cbSend   = _cmdSendResp,
                    .pArg       = req,
                };

                uint8_t len = ws_pkt.payload[0];
                uint8_t type = ws_pkt.payload[1];

                CMD_processMessage(&context, type, ws_pkt.payload+2, ws_pkt.len-2);
            }

            // Prepare response message
            //static const char * response_data = "Hello from server-12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234";
            //static const char * response_data = "Hello from server-1234567890";
            //sprintf(rsp, "Hello from server %d", count);
            //count++;
        } else if (ws_pkt.type == HTTPD_WS_TYPE_PING) {
            INFO("Got a WS PING frame, Replying PONG\n");
            ws_pkt.type = HTTPD_WS_TYPE_PONG;
        } else if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
            ws_pkt.len = 0;
            ws_pkt.payload = NULL;
        }

        INFO_BUF("sending frame",	PRINT_BUF_STYLE_HEX_SIZE_NL, ws_pkt.payload, ws_pkt.len);
        ret = httpd_ws_send_frame(req, &ws_pkt);
        if (ret != ESP_OK) {
            ERROR("httpd_ws_send_frame failed with %d\n", ret);
        }
        INFO("ws_handler: httpd_handle_t=%p, sockfd=%d, client_info:%d\n", req->handle,
                 httpd_req_to_sockfd(req), httpd_ws_get_fd_info(req->handle, httpd_req_to_sockfd(req)));
        free(buf);
        return ret;
    }
    free(buf);
    return ESP_OK;
}

esp_err_t wss_open_fd(httpd_handle_t hd, int sockfd)
{
    INFO("wss_open_fd %d\n", sockfd);
    wss_keep_alive_t h = httpd_get_global_user_ctx(hd);
    return wss_keep_alive_add_client(h, sockfd);
}

void wss_close_fd(httpd_handle_t hd, int sockfd)
{
    INFO("wss_close_fd %d\n", sockfd);
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

static const httpd_uri_t ws = {
        // .uri        = "/ws",
        .uri = "/",
        .method     = HTTP_GET,
        .handler    = ws_handler,
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

    // Configure the SSL parameters
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

    // Start the HTTP server with SSL
    esp_err_t ret = httpd_ssl_start(&server, &conf);
    if (ESP_OK != ret) {
        ERROR("Error starting server!\n");
        return NULL;
    }

    // Set URI handlers
    INFO("Registering URI handlers");
    httpd_register_uri_handler(server, &ws);
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
