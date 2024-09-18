#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*CMD_RESPONSE_CB)(void* pArg, uint8_t type, void* i_pBuf, uint16_t size);

typedef struct {
	CMD_RESPONSE_CB	p_cbSend;
	void*			pArg;
} CMD_CONTEXT;

bool	CMD_init(CMD_CONTEXT* i_pDefaultContext);
void	CMD_parseByte(CMD_CONTEXT* i_pContext, uint8_t data);
void	CMD_processMessage(CMD_CONTEXT* i_pContext, uint8_t type, uint8_t* i_pBuf, uint16_t size);
void	CMD_parseInit(void);
bool	CMD_setStreamContext(CMD_CONTEXT* i_pContext);

#ifdef __cplusplus
}
#endif
