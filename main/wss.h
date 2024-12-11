#pragma once

#include <sys_def.h>

#include <esp_https_server.h>

struct async_resp_arg {
	httpd_handle_t hd;
	int fd;
};

httpd_handle_t wss_start_server(void);
bool wss_send(struct async_resp_arg* i_pAsync, void* pBuf, size_t len);


