
#define DEF_DBG_MODULE	DBG_MODULE_CMD

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "argtable3/argtable3.h"

#include "esp_event.h"
#include "esp_check.h"
#include "soc/soc_caps.h"
#include "driver/gpio.h"
#include "driver/uart.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "main.h"
#include "cmd.h"
#include "cli.h"
#include "time.h"
#include "wifi.h"

// *INDENT-OFF*

//		Opcode name					OPCODE	Parameters
#define CMD(req, rsp)	\
	req(NOP,						0x00,	;)							\
	req(KEEPALIVE,					0x01,	;)							\
	rsp(KEEPALIVE,					0x02,	uint8_t	batVoltage;			\
											uint8_t	soc;)				\
	req(VER,						0x03,	;)							\
	rsp(VER,						0x04,	uint8_t		major;			\
											uint8_t		minor;			\
											uint8_t		hotfix;			\
											uint8_t		build;)			\
	req(STATUS,						0x05,	;)							\
	rsp(STATUS,						0x06,	uint16_t	pressure[4];	\
											uint8_t		valve[6];		\
											uint8_t		pump[6];		\
											uint8_t		voltage;		\
											uint8_t		soc;)			\
	req(SET_PRESSURE,				0x07,	uint16_t	pressure[4];)	\
	rsp(SET_PRESSURE,				0x08,	uint8_t		ok;)			\
	req(START_STREAM,				0x09,	;)							\
	rsp(STREAM,						0x0a,	uint64_t	time;			\
											uint16_t	pressure[4];)	\
	req(STOP_STREAM,				0x0b,	;)							\
	rsp(STOP_STREAM,				0x0c,	uint8_t		ok;)			\
	req(CONTROL_ENABLE,				0x0d,	uint8_t		on;)			\
	rsp(CONTROL_ENABLE,				0x0e,	uint8_t		ok;)			\




// *INDENT-ON*


#define CMD_ENUM_REQ(cmd, op, fields)	CMD_REQ_ ## cmd = (op),
#define CMD_ENUM_RSP(cmd, op, fields)	CMD_RSP_ ## cmd = (op),

#define CMD_REQ_BUF(c, op, fields)				\
	typedef  struct {							\
		fields									\
	} __attribute__((packed)) CMD_REQBUF_ ## c;

#define CMD_RSP_BUF(c, op, fields)					\
	typedef struct {								\
		fields										\
	} __attribute__((packed))  CMD_RSPBUF_ ## c;

#define CMD_DECLATE_FUNCS(cmd, op, fields)		\
	static bool _req_ ## cmd ## _func(CMD_CONTEXT* i_pContext, CMD_REQBUF_ ## cmd * i_pBuf, uint16_t size);

#define CMD_SWITCH(cmd, op, fields)											\
	case CMD_REQ_ ## cmd:													\
	TRACE_BUF(#cmd,	PRINT_BUF_STYLE_HEX_SIZE_NL, i_pBuf, size);			\
	retVal = _req_ ## cmd ## _func(pContext, (CMD_REQBUF_ ## cmd *)i_pBuf, size);	\
	break;


#define CMD_IS_VALID_SWITCH(cmd, op, fields)	\
	case CMD_REQ_ ## cmd:						\
	return true;


#define APP_RSP_PRINT_SWITCH(cmd, op, fields)									\
	case CMD_RSP_ ## cmd:	PRINT("%s(0x%x) ", STR(RSP_ ## cmd), op);	break;	\


#define APP_CMD_LIST_ARRAY(cmd, op, fields)	CMD_REQ_ ## cmd,
#define CMD_NONE(cmd, op, fields)

#define CMD_DECLARE_RSP_BUF(x, aditionalSize)					\
	uint8_t	rspBuf[sizeof(CMD_RSPBUF_ ## x) + aditionalSize];	\
	CMD_RSPBUF_ ## x* pRsp = (CMD_RSPBUF_ ## x*)rspBuf;			\


CMD(CMD_REQ_BUF, CMD_RSP_BUF)

typedef enum {
	CMD_REQ_INVALID,
	CMD(CMD_ENUM_REQ, CMD_NONE)
} CMD_REQ;

typedef enum {
	CMD(CMD_NONE, CMD_ENUM_RSP)
} CMD_RSP;

CMD(CMD_DECLATE_FUNCS, CMD_NONE)




typedef enum {
	CMD_STATE_WAIT_FOR_START,
	CMD_STATE_WAIT_FOR_TYPE,
	CMD_STATE_WAIT_FOR_LENGTH,
	CMD_STATE_WAIT_FOR_DATA,
} CMD_STATE;

static struct {
	CMD_CONTEXT		defaultContext;
	CMD_STATE	    state;
	COMM_TYPE		In_Message_Type;
	uint16_t		In_Message_Length;
	uint16_t		Total_Byte_Recieved;
	uint8_t			In_Message_Data[CMD_INCOMING_MESSAGE_MAX_SIZE];
} g_cmdDb;

static int imu_a = 1;
static int imu_g =1;



#if SIMULATION_MODE
	static uint8_t			_regsShadow[4][8][32];
	static const uint8_t	_regsDefault[32] = {0x3e, 0x96, 0xc0, 0x60, 0x00, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61};
#endif


static bool _init(void)
{
#if SIMULATION_MODE
	for (int i=0; i<4; i++) {
		for (int j=0; j<8; j++) {
			memcpy(&_regsShadow[i][j], _regsDefault, 32);
		}
	}
#endif

    return true;
}


bool _sendResp(CMD_CONTEXT* i_pContext, COMM_TYPE msgType, void* i_pBuf, uint8_t size)
{
	CMD_CONTEXT* pContext = i_pContext;

	if (NULL == i_pContext) {
		pContext = &g_cmdDb.defaultContext;
	}

	if (NULL == pContext) {
		ERROR("invalid context\n");
		return false;
	}

	if (NULL == pContext->p_cbSend) {
		return false;
	}

	if (!pContext->socket) {
		return false;
	}

	INFO_BUF("_sendResp",	PRINT_BUF_STYLE_HEX_SIZE_NL, i_pBuf, size);

	pContext->p_cbSend(pContext->socket, msgType, i_pBuf, size);

    return true;
}

static bool	_req_NOP_func(CMD_CONTEXT* i_pContext, CMD_REQBUF_NOP* i_pReq, uint16_t size)
{
	return true;
}

static bool	_req_KEEPALIVE_func(CMD_CONTEXT* i_pContext, CMD_REQBUF_KEEPALIVE* i_pReq, uint16_t size)
{
    CMD_RSPBUF_KEEPALIVE	rsp;
	
	INFO("KEEPALIVE\n");

	_sendResp(i_pContext, CMD_RSP_KEEPALIVE, &rsp, sizeof(rsp));

    return true;
}

static bool	_req_VER_func(CMD_CONTEXT* i_pContext, CMD_REQBUF_VER* i_pReq, uint16_t size)
{
	CMD_RSPBUF_VER	rsp;

    INFO("VER\n");

	_sendResp(i_pContext, CMD_RSP_VER, &rsp, sizeof(rsp));

	return true;
}

static bool	_req_STATUS_func(CMD_CONTEXT* i_pContext, CMD_REQBUF_STATUS* i_pReq, uint16_t size)
{
	CMD_RSPBUF_STATUS	rsp;

    INFO("STATUS\n");

    _sendResp(i_pContext, CMD_RSP_STATUS, &rsp, sizeof(rsp));
	
	return true;
}

static bool	_req_SET_PRESSURE_func(CMD_CONTEXT* i_pContext, CMD_REQBUF_SET_PRESSURE* i_pReq, uint16_t size)
{
    INFO("SET_PRESSURE\n");

	CMD_RSPBUF_SET_PRESSURE	rsp;

	_sendResp(i_pContext, CMD_RSP_SET_PRESSURE, &rsp, sizeof(rsp));

    return true;
}

static bool	_req_START_STREAM_func(CMD_CONTEXT* i_pContext, CMD_REQBUF_START_STREAM* i_pReq, uint16_t size)
{
	INFO("START_STREAM\n");
    return true;
}

static bool	_req_STOP_STREAM_func(CMD_CONTEXT* i_pContext, CMD_REQBUF_STOP_STREAM* i_pReq, uint16_t size)
{
	INFO("STOP_STREAM\n");

	CMD_RSPBUF_STOP_STREAM	rsp;

	_sendResp(i_pContext, CMD_RSP_STOP_STREAM, &rsp, sizeof(rsp));

    return true;
}

static bool	_req_CONTROL_ENABLE_func(CMD_CONTEXT* i_pContext, CMD_REQBUF_CONTROL_ENABLE* i_pReq, uint16_t size)
{
	INFO("CONTROL_ENABLE\n");

	CMD_RSPBUF_CONTROL_ENABLE	rsp;

	_sendResp(i_pContext, CMD_RSP_CONTROL_ENABLE, &rsp, sizeof(rsp));

    return true;
}


static bool _isValidMsgType(uint8_t type)
{
	switch (type) {
		CMD(CMD_IS_VALID_SWITCH, CMD_NONE)
	}

	return false;
}

static void _parsingInit(void)
{
	g_cmdDb.state               = CMD_STATE_WAIT_FOR_START;
	g_cmdDb.In_Message_Type     = CMD_REQ_INVALID;
	g_cmdDb.In_Message_Length   = 0;
	g_cmdDb.Total_Byte_Recieved = 0;
}

void CMD_processMessage(CMD_CONTEXT* i_pContext, uint8_t type, uint8_t* i_pBuf, uint16_t size)
{
	bool	retVal = false;

	CMD_CONTEXT* pContext;
	CMD_REQ	t = (CMD_REQ)type;

	if (NULL == i_pContext) {
		pContext = &g_cmdDb.defaultContext;
	} else {
		pContext = i_pContext;
	}

	if (!pContext) {
		ERROR("invalid context\n");
		return;
	}

	TRACE_BUF("cmd",	PRINT_BUF_STYLE_HEX_SIZE_NL, i_pBuf, size);

	switch (t) {
		CMD(CMD_SWITCH, CMD_NONE)

		default:
			WARN("unhandled msg 0x%02x: ", type);
			WARN_BUF("",	PRINT_BUF_STYLE_HEX_SIZE_NL, i_pBuf, size);
	}

	if (false == retVal) {
		WARN("cmd handler returned error\n");
	}
}

void CMD_parseByte(CMD_CONTEXT* i_pContext, uint8_t data)
{
    switch (g_cmdDb.state) {
        case CMD_STATE_WAIT_FOR_START:
            if (data == START_MESSAGE_CHARACTER) {
                _parsingInit();
                g_cmdDb.state = CMD_STATE_WAIT_FOR_TYPE;
            }
        break;

        case CMD_STATE_WAIT_FOR_TYPE:
            if (_isValidMsgType(data)) {
                TRACE("valid type %02x\n", data);
                g_cmdDb.In_Message_Type = (COMM_TYPE)data;
                g_cmdDb.state = CMD_STATE_WAIT_FOR_LENGTH;
            }
            else {
                WARN("invalid msg type %02x\n", data);
                _parsingInit();
            }
        break;

        case CMD_STATE_WAIT_FOR_LENGTH:
            TRACE("length %02x\n", data);

            if (data == 0) {
                CMD_processMessage(i_pContext, g_cmdDb.In_Message_Type, g_cmdDb.In_Message_Data, g_cmdDb.In_Message_Length);
                _parsingInit();
            } else if (data < CMD_INCOMING_MESSAGE_MAX_SIZE) {
                g_cmdDb.In_Message_Length   = data;
                g_cmdDb.state       = CMD_STATE_WAIT_FOR_DATA;
                g_cmdDb.Total_Byte_Recieved = 0;
            } else {
                _parsingInit();
            }
        break;

        case CMD_STATE_WAIT_FOR_DATA:
            TRACE("data %02x[%02x] of %02x\n", data, g_cmdDb.Total_Byte_Recieved, g_cmdDb.In_Message_Length);

            g_cmdDb.In_Message_Data[g_cmdDb.Total_Byte_Recieved] = data;
            g_cmdDb.Total_Byte_Recieved++;

            if (g_cmdDb.Total_Byte_Recieved >= g_cmdDb.In_Message_Length) {
                TRACE("CMD_processMessage\n");
                CMD_processMessage(i_pContext, g_cmdDb.In_Message_Type, g_cmdDb.In_Message_Data, g_cmdDb.In_Message_Length);
                _parsingInit();
            }
        break;

        default:
            _parsingInit();
    }
}

static bool dbgStatus(uint8_t argc, char** argv)
{
    return true;
}

static bool dbgImuSim(uint8_t argc, char** argv)
{
	if (argc < 3) {
		PRINT("g=%d, a=%d\n", imu_g, imu_a);
		return true;
	}

    imu_g = strtoul(argv[1], NULL, 10);
    imu_a = strtoul(argv[1], NULL, 10);

    return true;
}

DEBUG_MENU_START(g_menu)
    DEBUG_MENU_DIR("cmd", NULL)
	    DEBUG_MENU_CMD("status",		NULL,		NULL, dbgStatus)
	    DEBUG_MENU_CMD("imuSim",		"<a> <g>",		NULL, dbgImuSim)
    DEBUG_MENU_DIR_END
DEBUG_MENU_END


bool  CMD_init(CMD_CONTEXT* i_pDefaultContext)
{
	DBG_TREE_add("/", g_menu);

#if SIMULATION_MODE
//	for (int i=0; i<4; i++) {
//		for (int j=0; j<8; j++) {
//			memcpy(&_regsShadow[i][j], _regsDefault, 32);
//		}
//	}
#endif

	if (i_pDefaultContext) {
		memcpy(&g_cmdDb.defaultContext, i_pDefaultContext, sizeof(g_cmdDb.defaultContext));
	} else {
		memset(&g_cmdDb.defaultContext, 0, sizeof(g_cmdDb.defaultContext));
	}

    _init();

	return true;
}