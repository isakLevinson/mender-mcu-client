#pragma once

void    OTA_init(void);
int		OTA_auto(char* pUrl);
bool	OTA_begin(char* pUrl);
bool	OTA_perform(void);
void	OTA_restart(void);