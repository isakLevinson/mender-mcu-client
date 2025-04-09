#pragma once

#include <sys_def.h>

#include <esp_https_server.h>

struct async_resp_arg {
	httpd_handle_t hd;
	int fd;
};

bool	wss_init(void);
bool	wss_config_start(void);
bool	wss_config_stop(void);
bool	wss_send(struct async_resp_arg* i_pAsync, void* pBuf, size_t len);


