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

#define Shimon

#if !CONFIG_HTTPD_WS_SUPPORT
#error This example cannot be used unless HTTPD_WS_SUPPORT is enabled in esp-http-server component configuration
#endif

static void wss_server_send_messages(httpd_handle_t* server);

struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
};

static const size_t max_clients = 4;

static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        INFO("Handshake done, the new connection was opened");
        return ESP_OK;
    }
    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

    // First receive the full ws message
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ERROR("httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    INFO("frame len is %d", ws_pkt.len);
    if (ws_pkt.len) {
        buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            ERROR("Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ERROR("httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
    }
    if (ws_pkt.type == HTTPD_WS_TYPE_PONG) {
        INFO("Received PONG message");
        free(buf);
        //return wss_keep_alive_client_is_active(httpd_get_global_user_ctx(req->handle),
        //        httpd_req_to_sockfd(req));
        return 0;

    } else if (ws_pkt.type == HTTPD_WS_TYPE_TEXT || ws_pkt.type == HTTPD_WS_TYPE_PING || ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
            INFO("Received packet with message: %s", ws_pkt.payload);
            // Prepare response message
            static const char * response_data = "Hello from server-12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234";
            httpd_ws_frame_t response_pkt;
            memset(&response_pkt, 0, sizeof(httpd_ws_frame_t));
            response_pkt.payload = (uint8_t*)response_data;
            response_pkt.len = strlen(response_data);
            response_pkt.type = HTTPD_WS_TYPE_TEXT;

            // Send response message
               // Get the current time
            TickType_t startTime = xTaskGetTickCount();
            int sent_count = 0;
            // Loop for 10 seconds
            INFO("start sending data at %lu:", startTime);
            while ((xTaskGetTickCount() - startTime) < (10 * configTICK_RATE_HZ)) {
        // Create a response structure
                ret = httpd_ws_send_frame(req, &response_pkt);
                if (ret != ESP_OK) {
                    ERROR("httpd_ws_send_frame failed with %d", ret);
                }
                sent_count += response_pkt.len;
                // INFO("sent data at %lu:", xTaskGetTickCount());
            }
            INFO("end sending data at %lu:", xTaskGetTickCount());
            INFO("sent %d bytes", sent_count);
            // static httpd_handle_t server = NULL;
            // wss_server_send_messages(&server);

        } else if (ws_pkt.type == HTTPD_WS_TYPE_PING) {
            INFO("Got a WS PING frame, Replying PONG");
            ws_pkt.type = HTTPD_WS_TYPE_PONG;
        } else if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
            ws_pkt.len = 0;
            ws_pkt.payload = NULL;
        }
        ret = httpd_ws_send_frame(req, &ws_pkt);
        if (ret != ESP_OK) {
            ERROR("httpd_ws_send_frame failed with %d", ret);
        }
        INFO("ws_handler: httpd_handle_t=%p, sockfd=%d, client_info:%d", req->handle,
                 httpd_req_to_sockfd(req), httpd_ws_get_fd_info(req->handle, httpd_req_to_sockfd(req)));
        free(buf);
        return ret;
    }
    free(buf);
    return ESP_OK;
}

esp_err_t wss_open_fd(httpd_handle_t hd, int sockfd)
{
    INFO("New client connected %d", sockfd);
    wss_keep_alive_t h = httpd_get_global_user_ctx(hd);
    return wss_keep_alive_add_client(h, sockfd);
}

void wss_close_fd(httpd_handle_t hd, int sockfd)
{
    INFO("Client disconnected %d", sockfd);
    wss_keep_alive_t h = httpd_get_global_user_ctx(hd);
    wss_keep_alive_remove_client(h, sockfd);
    close(sockfd);
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

static void send_hello(void *arg)
{
    static const char * data = "Hello client";
    struct async_resp_arg *resp_arg = arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t*)data;
    ws_pkt.len = strlen(data);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    httpd_ws_send_frame_async(hd, fd, &ws_pkt);
    free(resp_arg);
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
    ERROR("Client not alive, closing fd %d", fd);
    httpd_sess_trigger_close(wss_keep_alive_get_user_ctx(h), fd);
    return true;
}

bool check_client_alive_cb(wss_keep_alive_t h, int fd)
{
    TRACE("Checking if client (fd=%d) is alive", fd);
    struct async_resp_arg *resp_arg = malloc
(sizeof(struct async_resp_arg));
    resp_arg->hd = wss_keep_alive_get_user_ctx(h);
    resp_arg->fd = fd;

    if (httpd_queue_work(resp_arg->hd, send_ping, resp_arg) == ESP_OK) {
        return true;
    }
    return false;
}

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
        INFO("Error starting server!");
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

// Get all clients and send async message
static void wss_server_send_messages(httpd_handle_t* server)
{
    const char *data = "X"; // Use a single character to generate 1024-byte message
    char *message = malloc(1025); // Allocate memory for the message
    if (message == NULL) {
        ERROR("Failed to allocate memory for message");
        return;
    }
    memset(message, 0, 1025);
    for (int i = 0; i < 1024; i++) {
        message[i] = *data; // Fill the message with the character
    }

    // Get the current time
    TickType_t startTime = xTaskGetTickCount();

    // Loop for 10 seconds
    while ((xTaskGetTickCount() - startTime) < (10 * configTICK_RATE_HZ)) {
        // Create a response structure
        struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
        if (resp_arg == NULL) {
            ERROR("Failed to allocate memory for response argument");
            free(message);
            return;
        }
        resp_arg->hd = *server;

        // Send 1024 bytes message to the connected client
        httpd_ws_frame_t ws_pkt;
        memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
        ws_pkt.payload = (uint8_t*)message; // Set the message payload
        ws_pkt.len = 1024; // Set the message length
        ws_pkt.type = HTTPD_WS_TYPE_TEXT;

        // Send the message to each connected client
        size_t clients = max_clients;
        int    client_fds[max_clients];
        if (httpd_get_client_list(*server, &clients, client_fds) == ESP_OK) {
            for (size_t i = 0; i < clients; ++i) {
                int sockfd = client_fds[i];
                if (httpd_ws_get_fd_info(*server, sockfd) == HTTPD_WS_CLIENT_WEBSOCKET) {
                    INFO("Sending 1024 bytes message to connected client (fd=%d)", sockfd);
                    resp_arg->fd = sockfd;
                    if (httpd_queue_work(*server, send_hello, resp_arg) != ESP_OK) {
                        ERROR("httpd_queue_work failed!");
                        free(resp_arg);
                        free(message);
                        return;
                    }
                }
            }
        } else {
            ERROR("httpd_get_client_list failed!");
        }

        // Free allocated memory
        free(resp_arg);

        // Delay for a short period before sending the next message
        vTaskDelay(1000 / portTICK_PERIOD_MS); // 1 second delay
    }

    // Free allocated memory
    free(message);
}

#if 0
void app_main(void)
{
    static httpd_handle_t server = NULL;

    // Initialize NVS
    ESP_ERROR_CHECK(nvs_flash_init());

    // Initialize TCP/IP network stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Register event handlers to start server when Wi-Fi or Ethernet is connected,
     * and stop server when disconnection happens.
     */
#ifdef CONFIG_EXAMPLE_CONNECT_WIFI
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
#endif // CONFIG_EXAMPLE_CONNECT_WIFI
#ifdef CONFIG_EXAMPLE_CONNECT_ETHERNET
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, &disconnect_handler, &server));
#endif // CONFIG_EXAMPLE_CONNECT_ETHERNET

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    /* This function demonstrates periodic sending Websocket messages
     * to all connected clients to this server
     */
    // wss_server_send_messages(&server);
}
#endif