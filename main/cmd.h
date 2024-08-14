#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// TODO: move to C file
#define START_MESSAGE_CHARACTER			0x55


typedef enum {
	COMM_TYPE_START_SESSION	= 0x41,
	COMM_TYPE_DATA			= 0x42,
	COMM_TYPE_IMU			= 0x43,
	COMM_TYPE_TIMESTAMP		= 0x44,
} COMM_TYPE;


typedef bool (*CMD_RESPONSE_CB)(void* pArg, COMM_TYPE Message_Type, void* i_pBuf, uint16_t size);

typedef struct {
	CMD_RESPONSE_CB	p_cbSend;
	void*			pArg;
//	int     		socket;
} CMD_CONTEXT;

bool	CMD_init(CMD_CONTEXT* i_pDefaultContext);
void	CMD_parseByte(CMD_CONTEXT* i_pContext, uint8_t data);
void	CMD_processMessage(CMD_CONTEXT* i_pContext, uint8_t type, uint8_t* i_pBuf, uint16_t size);
void	CMD_parseInit(void);

#ifdef __cplusplus
}
#endif
