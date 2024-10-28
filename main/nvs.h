#pragma once

void NVS_init(void);


bool NVS_set_ssid(char* ssid, char* passwd);
bool NVS_get_ssid(char* ssid, char* passwd);