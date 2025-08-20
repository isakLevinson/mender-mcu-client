#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*CMD_RESPONSE_CB)(void* pArg, void* i_pBuf, uint16_t size);
typedef bool (*CMD_STATUS_CB)(void* pArg, uint32_t status);
typedef bool (*CMD_KA_CB)(uint8_t timeout);

typedef struct {
	CMD_RESPONSE_CB	p_cbSend;
	CMD_STATUS_CB	p_cbStatus;
	CMD_KA_CB		p_cbKa;
	void*			pArg;
} CMD_CONTEXT;

bool	CMD_init(CMD_CONTEXT* i_pDefaultContext);
void	CMD_parseByte(CMD_CONTEXT* i_pContext, uint8_t data);
bool	CMD_processBuffer(CMD_CONTEXT* i_pContext, uint8_t* i_pBuf, uint16_t size);
bool 	CMD_processJson(CMD_CONTEXT* i_pContext, const char* pCommand, char* pData);
void	CMD_parseInit(void);
bool	CMD_setStreamContext(CMD_CONTEXT* i_pContext);
bool	CMD_sendBatteryEvent(uint8_t soc, uint16_t voltage_mv);
bool	CMD_sendOtaStatusEvent(void);

#ifdef __cplusplus
}
#endif
