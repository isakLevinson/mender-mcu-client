#pragma once

void    MENDER_init(void);
bool	MENDER_version(char** ppProjName, char** ppVer, uint32_t* pNumbers);
void	MENDER_execute(void);
