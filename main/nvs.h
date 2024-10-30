#pragma once

#include <sys_def.h>

void NVS_init(void);
bool NVS_set_ssid(char* ssid, char* passwd);
bool NVS_get_ssid(char* ssid, char* passwd);
bool NVS_get_certificate(char* val);
bool NVS_set_certificate(char* val);
bool NVS_get_sync_dns(char* val);
bool NVS_set_sync_dns(char* val);
bool NVS_get_sync_port(char* val);
bool NVS_set_sync_port(char* val);
bool NVS_get_mdns(char* val);
bool NVS_set_mdns(char* val);
