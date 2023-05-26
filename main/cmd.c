
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



// *INDENT-OFF*

//		Opcode name					OPCODE	Parameters
#define CMD(req, rsp)	\
	req(VER,						0x01,	;)		\
	rsp(VER,						0x01,	uint8_t		swMagor;	\
											uint8_t		swMinor;	\
											uint8_t		swPatch;    \
											uint8_t		hwMagor;	\
											uint8_t		hwMinor;	\
											uint32_t	ip;			\
											uint32_t	guid[3];)	\
	req(REG_WRITE,					0x11,	uint8_t		spiAndChannel;	\
											uint8_t		typeAndAddress;	\
											uint8_t		data[];)		\
	rsp(REG_WRITE,					0x11,	uint8_t		spiAndChannel;	\
											uint8_t		typeAndAddress;	\
											uint8_t		regs;)			\
	req(REG_READ,					0x21,	uint8_t		spiAndChannel;	\
											uint8_t		firstReg;		\
											uint8_t		count;)			\
	rsp(REG_READ,					0x21,	uint8_t		spiAndChannel;	\
											uint8_t		firstReg;		\
											uint8_t		data[];)		\
	req(START,						0x31,	uint16_t	interval;	\
											uint8_t		channelBitmask[4];	\
											uint8_t		isSim;				\
											uint8_t		simMode;)			\
	rsp(START,						0x31,	uint8_t		totalModules;)		\
	req(TURN_ON,					0x51,	;)							\
	rsp(TURN_ON,					0x51,	uint8_t		notMeasuring;)		\
	req(TURN_OFF,					0x52,	;)							\
	rsp(TURN_OFF,					0x52,	uint8_t		notMeasuring;)		\
	req(RESET,						0x54,	;)							\
	rsp(RESET,						0x54,	uint8_t		notMeasuring;)		\
	req(SPI_SPEED,					0x53,	uint8_t		speed;)				\
	rsp(SPI_SPEED,					0x53,	uint8_t		isOk;)				\
	req(SYNC_START,					0x56,	;)							\
	rsp(SYNC_START,					0x56,	uint8_t		dummy;)				\
	req(TIME_SYNC,					0x57,	uint64_t	time;)				\
	rsp(TIME_SYNC,					0x57,	uint64_t	requestTime;		\
											uint64_t	currentTime1;		\
											uint64_t	currentTime2;)		\
	req(IMU_START,					0x58,	uint8_t		ascale;				/* 0-2G, 1-16G, 2-4G, 3-8G */					\
											uint8_t		gscale;)			/* 0-250dps, 1-500dps, 2-1000dps, 3-2000dps */	\
	rsp(IMU_START,					0x58,	uint8_t		ascale;				/* 0-2G, 1-16G, 2-4G, 3-8G */					\
											uint8_t		gscale;)			/* 0-250dps, 1-500dps, 2-1000dps, 3-2000dps */	\
	rsp(NTP,						0x59,	uint64_t	sysTime;			\
											uint64_t	ntpTime;)			\


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


static bool _init(void)
{
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

	pContext->p_cbSend(pContext->socket, msgType, i_pBuf, size);

    return true;
}


static bool	_req_VER_func(CMD_CONTEXT* i_pContext, CMD_REQBUF_VER* i_pReq, uint16_t size)
{
    //esp_err_t   ret;
	CMD_RSPBUF_VER	rsp;
    //esp_flash_t chip;

    INFO("VER\n");

	rsp.swMagor	= SOFTWARE_MAJOR_VERSION;
	rsp.swMinor	= SOFTWARE_MINOR_VERSION;
	rsp.swPatch = SOFTWARE_PATCH_VERSION;
	rsp.hwMagor	= HARDWARE_MAJOR_VERSION;
	rsp.hwMinor	= HARDWARE_MINOR_VERSION;
	
    // TODO: use actual IP address
    rsp.ip = 0x11223344;
	
    // TODO:     use actual GUID
    //esp_err_t esp_flash_init(&chip);
    //ret = esp_flash_read_unique_chip_id(esp_flash_t *chip, uint64_t* out_uid)
    rsp.guid[0]	= 0x12;
	rsp.guid[1]	= 0x34;
	rsp.guid[2]	= 0x56;

	_sendResp(i_pContext, CMD_RSP_VER, &rsp, sizeof(rsp));

	return true;
}

static bool	_req_REG_WRITE_func(CMD_CONTEXT* i_pContext, CMD_REQBUF_REG_WRITE* i_pReq, uint16_t size)
{
    INFO("REG_WRITE\n");
    return true;
}

static bool	_req_REG_READ_func(CMD_CONTEXT* i_pContext, CMD_REQBUF_REG_READ* i_pReq, uint16_t size)
{
    INFO("REG_READ\n");
    return true;
}

static bool	_req_START_func(CMD_CONTEXT* i_pContext, CMD_REQBUF_START* i_pReq, uint16_t size)
{
    INFO("START\n");
    return true;
}

static bool	_req_TURN_ON_func(CMD_CONTEXT* i_pContext, CMD_REQBUF_TURN_ON* i_pReq, uint16_t size)
{
    INFO("TURN_ON\n");
    return true;
}

static bool	_req_TURN_OFF_func(CMD_CONTEXT* i_pContext, CMD_REQBUF_TURN_OFF* i_pReq, uint16_t size)
{
    INFO("TURN_OFF\n");
    return true;
}

static bool	_req_RESET_func(CMD_CONTEXT* i_pContext, CMD_REQBUF_RESET* i_pReq, uint16_t size)
{
    INFO("RESET\n");
    return true;
}

static bool	_req_SPI_SPEED_func(CMD_CONTEXT* i_pContext, CMD_REQBUF_SPI_SPEED* i_pReq, uint16_t size)
{
    INFO("SPI_SPEED\n");
    return true;
}

static bool	_req_SYNC_START_func(CMD_CONTEXT* i_pContext, CMD_REQBUF_SYNC_START* i_pReq, uint16_t size)
{
    INFO("SYNC_START\n");
    return true;
}

static bool	_req_TIME_SYNC_func(CMD_CONTEXT* i_pContext, CMD_REQBUF_TIME_SYNC* i_pReq, uint16_t size)
{
    INFO("TIME_SYNC\n");
    return true;
}

static bool	_req_IMU_START_func(CMD_CONTEXT* i_pContext, CMD_REQBUF_IMU_START* i_pReq, uint16_t size)
{
    INFO("IMU_START\n");
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

DEBUG_MENU_START(g_menu)
    DEBUG_MENU_DIR("cmd", NULL)
	    DEBUG_MENU_CMD("status",		NULL,		NULL, dbgStatus)
    DEBUG_MENU_DIR_END
DEBUG_MENU_END


bool  CMD_init(CMD_CONTEXT* i_pDefaultContext)
{
	DBG_TREE_add("/", g_menu);

#if SIMULATION_MODE
	for (int i=0; i<4; i++) {
		for (int j=0; j<8; j++) {
			memcpy(&_regsShadow[i][j], _regsDefault, 32);
		}
	}
#endif

	if (i_pDefaultContext) {
		memcpy(&g_cmdDb.defaultContext, i_pDefaultContext, sizeof(g_cmdDb.defaultContext));
	} else {
		memset(&g_cmdDb.defaultContext, 0, sizeof(g_cmdDb.defaultContext));
	}

    _init();

	return true;
}