
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
#include "freertos/semphr.h"

#include "main.h"
#include "cmd.h"
#include "cli.h"
#include "time.h"
#include "wifi.h"
#include "app.h"

// *INDENT-OFF*

//		Opcode name					OPCODE	Parameters
#define CMD(req, rsp)	\
	req(NOP,						0x00,	;)							\
	req(KEEPALIVE,					0x01,	;)							\
	rsp(KEEPALIVE,					0x02,	uint8_t	batVoltage;			\
											uint8_t	soc;)				\
	req(VER,						0x03,	;)							\
	rsp(VER,						0x04,	uint8_t		hash[8];		\
											uint8_t		sw[3];			\
											uint8_t		hw[3];)			\
	req(STATUS,						0x05,	;)							\
	rsp(STATUS,						0x06,	uint16_t	pressure[4];	\
											uint8_t		valve[4];		\
											uint8_t		pump[4];		\
											uint8_t		voltage;		\
											uint8_t		soc;)			\
	req(SET_PRESSURE,				0x07,	uint16_t	pressure[4];)	\
	rsp(SET_PRESSURE,				0x08,	uint8_t		ok;)			\
	req(START_STREAM,				0x09,	;)							\
	rsp(START_STREAM,				0x0a,	uint8_t		ok;)			\
	req(STOP_STREAM,				0x0b,	;)							\
	rsp(STOP_STREAM,				0x0c,	uint8_t		ok;)			\
	req(CONTROL_ENABLE,				0x0d,	uint8_t		on;)			\
	rsp(CONTROL_ENABLE,				0x0e,	uint8_t		ok;)			\
	req(SET_PUMPS,					0x0f,	int8_t		on[4];)			\
	rsp(SET_PUMPS,					0x10,	uint8_t		ok;)			\
	req(SET_VALVES,					0x11,	int8_t		on[4];)			\
	rsp(SET_VALVES,					0x12,	uint8_t		ok;)			\
	rsp(STREAM,						0x13,	uint64_t	time;			\
											uint16_t	pressure[4];)	\

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
	TRACE1_BUF(#cmd,	PRINT_BUF_STYLE_HEX_SIZE_NL, i_pBuf, size);			\
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
	CMD_STATE_WAIT_FOR_LENGTH0,
	CMD_STATE_WAIT_FOR_LENGTH1,
	CMD_STATE_WAIT_FOR_DATA,
} CMD_STATE;

static struct {
	SemaphoreHandle_t	semaphore;
	CMD_CONTEXT			streamContext;
	CMD_STATE	    	state;
	uint16_t			expectedLength;
	uint16_t			received;
	uint8_t				rxBuf[CMD_INCOMING_MESSAGE_MAX_SIZE];
	uint32_t			streamPeriod;
	int32_t				streamSentTime;
} g_cmd;


bool _sendResp(CMD_CONTEXT* i_pContext, COMM_TYPE msgType, void* i_pBuf, uint8_t size)
{
	CMD_CONTEXT* pContext = i_pContext;

	if (!i_pContext) {
		INFO("context is NULL\n");
		pContext = &g_cmd.streamContext;
	}

	if (!pContext) {
		ERROR("invalid context\n");
		return false;
	}

	if (!pContext->p_cbSend) {
		ERROR("p_cbSend is NULL\n");
		return false;
	}

	xSemaphoreTake(g_cmd.semaphore, portMAX_DELAY);

	TRACE_BUF("_sendResp",	PRINT_BUF_STYLE_HEX_SIZE_NL, i_pBuf, size);

	pContext->p_cbSend(pContext->pArg, msgType, i_pBuf, size);

	xSemaphoreGive(g_cmd.semaphore);

    return true;
}

static void _taskStreamer(void *arg)
{
	int32_t	t;
	int16_t	press[4];
	CMD_RSPBUF_STREAM	rsp;
	uint32_t	i;

    while (true) {
        vTaskDelay(10);
		if (!g_cmd.streamPeriod) {
			continue;
		}		

		t = TIME_get32();
		if (t - g_cmd.streamSentTime < g_cmd.streamPeriod) {
			continue;
		}

		g_cmd.streamSentTime += g_cmd.streamPeriod;

		APP_getPressure(press);
		TRACE("stream %d: %3d %3d %3d %3d\n", t, press[0], press[1],press[2], press[3]);

		rsp.time = t;
		for (i=0; i<4; i++) {
			rsp.pressure[i] = press[i];
		}
		_sendResp(&g_cmd.streamContext, CMD_RSP_STREAM, &rsp, sizeof(rsp));
    }
}

static bool _init(void)
{
	bool	ret;

	g_cmd.semaphore = xSemaphoreCreateBinary();
	xSemaphoreGive(g_cmd.semaphore);

	ret = xTaskCreate(_taskStreamer, "streamer", 8192, NULL, 3, NULL);
    if (ret != pdPASS) {
        ERROR("create task %s failed\n", "streamer");
        return false;
    }

    return true;
}

static void _streamPeriod(uint32_t period)
{
	g_cmd.streamSentTime = TIME_get32();
	g_cmd.streamPeriod = period;
}

static bool	_req_NOP_func(CMD_CONTEXT* i_pContext, CMD_REQBUF_NOP* i_pReq, uint16_t size)
{
	return true;
}

static bool	_req_KEEPALIVE_func(CMD_CONTEXT* i_pContext, CMD_REQBUF_KEEPALIVE* i_pReq, uint16_t size)
{
    CMD_RSPBUF_KEEPALIVE	rsp;
	
	INFO("KEEPALIVE\n");

	// TODO: use real values
	rsp.batVoltage	= 37;
	rsp.soc			= 85;
	_sendResp(i_pContext, CMD_RSP_KEEPALIVE, &rsp, sizeof(rsp));

    return true;
}

static bool	_req_VER_func(CMD_CONTEXT* i_pContext, CMD_REQBUF_VER* i_pReq, uint16_t size)
{
	CMD_RSPBUF_VER	rsp;

    INFO("VER\n");

	memset(rsp.hash, 0, sizeof(rsp.hash));
	rsp.sw[0] = SW_VERSION_MAJOR;
	rsp.sw[1] = SW_VERSION_MINOR;
	rsp.sw[2] = SW_VERSION_BUILD;

	rsp.hw[0] = HW_VERSION_MAJOR;
	rsp.hw[1] = HW_VERSION_MINOR;
	rsp.hw[2] = HW_VERSION_BUILD;

	_sendResp(i_pContext, CMD_RSP_VER, &rsp, sizeof(rsp));

	return true;
}

static bool	_req_STATUS_func(CMD_CONTEXT* i_pContext, CMD_REQBUF_STATUS* i_pReq, uint16_t size)
{
	CMD_RSPBUF_STATUS	rsp;
	int16_t	press[4];
	bool	valves[5];
	bool	pumps[4];
	int		i;

    INFO("STATUS\n");

	APP_getPressure(press);
	for (i=0; i<4; i++) {
		rsp.pressure[i] = press[i];
	}

	APP_getValves(valves);
	for (i=0; i<4; i++) {
		rsp.valve[i] = valves[i];
	}

	APP_getPump(pumps);
	for (i=0; i<4; i++) {
		rsp.pump[i] = pumps[i];
	}

	// TODO: use real values
	rsp.voltage = 36;
	rsp.soc		= 90;

    _sendResp(i_pContext, CMD_RSP_STATUS, &rsp, sizeof(rsp));
	
	return true;
}

static bool	_req_SET_PRESSURE_func(CMD_CONTEXT* i_pContext, CMD_REQBUF_SET_PRESSURE* i_pReq, uint16_t size)
{
    INFO("SET_PRESSURE\n");
	uint16_t	press[4];
	uint8_t i;

	CMD_RSPBUF_SET_PRESSURE	rsp;

	for (i=0; i<4; i++) {
		press[i] = i_pReq->pressure[i];
	}

	APP_setTarget(press);

	rsp.ok = 1;
	_sendResp(i_pContext, CMD_RSP_SET_PRESSURE, &rsp, sizeof(rsp));

    return true;
}

static bool	_req_START_STREAM_func(CMD_CONTEXT* i_pContext, CMD_REQBUF_START_STREAM* i_pReq, uint16_t size)
{
	INFO("START_STREAM\n");
	bool	okToStream = true;

	CMD_RSPBUF_SET_PRESSURE	rsp;

	if (!g_cmd.streamContext.p_cbSend) {
		WARN("p_cbSend is NULL\n");
		okToStream = false;
	}
	if (!g_cmd.streamContext.pArg) {
		WARN("pArg is NULL\n");
		okToStream = false;
	}

	rsp.ok = okToStream? 1:0;
	_sendResp(i_pContext, CMD_RSP_STOP_STREAM, &rsp, sizeof(rsp));

	if (okToStream) {
		_streamPeriod(100);
	}
    return true;
}

static bool	_req_STOP_STREAM_func(CMD_CONTEXT* i_pContext, CMD_REQBUF_STOP_STREAM* i_pReq, uint16_t size)
{
	INFO("STOP_STREAM\n");

	CMD_RSPBUF_STOP_STREAM	rsp;

	_streamPeriod(0);

	rsp.ok = 1;
	_sendResp(i_pContext, CMD_RSP_STOP_STREAM, &rsp, sizeof(rsp));

    return true;
}

static bool	_req_CONTROL_ENABLE_func(CMD_CONTEXT* i_pContext, CMD_REQBUF_CONTROL_ENABLE* i_pReq, uint16_t size)
{
	INFO("CONTROL_ENABLE\n");

	CMD_RSPBUF_CONTROL_ENABLE	rsp;

	APP_loopEnable(i_pReq->on);

	rsp.ok = 1;
	_sendResp(i_pContext, CMD_RSP_CONTROL_ENABLE, &rsp, sizeof(rsp));

    return true;
}

static bool	_req_SET_PUMPS_func(CMD_CONTEXT* i_pContext, CMD_REQBUF_SET_PUMPS* i_pReq, uint16_t size)
{
	INFO("SET_PUMPS\n");
	bool	ret;
	uint8_t	i;
	uint8_t	ok = true;

	CMD_RSPBUF_SET_PUMPS	rsp;

	for (i=0; i<4; i++) {
		switch (i_pReq->on[i]) {
			case 0: ret = APP_setPump(i, false);	break;
			case 1: ret = APP_setPump(i, true);		break;
			default:
				ret = true;
		}
		if (!ret) {
			ok = false;
		}
	}

	rsp.ok = ok;

	_sendResp(i_pContext, CMD_RSP_SET_PUMPS, &rsp, sizeof(rsp));

    return true;
}

static bool	_req_SET_VALVES_func(CMD_CONTEXT* i_pContext, CMD_REQBUF_SET_VALVES* i_pReq, uint16_t size)
{
	INFO("SET_VALVES\n");
	bool	ret;
	uint8_t	i;
	uint8_t	ok = true;

	CMD_RSPBUF_SET_VALVES	rsp;

	for (i=0; i<4; i++) {
		switch (i_pReq->on[i]) {
			case 0: ret = APP_setValve(i, false);	break;
			case 1: ret = APP_setValve(i, true);	break;
			default:
				ret = true;
		}
		if (!ret) {
			ok = false;
		}
	}

	rsp.ok = ok;

	_sendResp(i_pContext, CMD_RSP_SET_VALVES, &rsp, sizeof(rsp));

    return true;
}

static bool _isValidMsgType(uint8_t type)
{
	switch (type) {
		CMD(CMD_IS_VALID_SWITCH, CMD_NONE)
	}

	return false;
}

void CMD_parseInit(void)
{
	TRACE1("CMD_parseInit\n");
	g_cmd.state             = CMD_STATE_WAIT_FOR_LENGTH0;
	g_cmd.expectedLength  	= 0;
	g_cmd.received			= 0;
}

void CMD_processMessage(CMD_CONTEXT* i_pContext, uint8_t type, uint8_t* i_pBuf, uint16_t size)
{
	bool	retVal = false;

	CMD_CONTEXT* pContext;
	CMD_REQ	t = (CMD_REQ)type;

	if (!i_pContext) {
		return;
	}
		
	pContext = i_pContext;

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
	TRACE1("c:%02x state:%d expected:%04x, rec:%x\n", data, g_cmd.state, g_cmd.expectedLength, g_cmd.received);

    switch (g_cmd.state) {
         case CMD_STATE_WAIT_FOR_LENGTH0:
            TRACE1("length0 %02x\n", data);
			g_cmd.expectedLength = data;
			g_cmd.state = CMD_STATE_WAIT_FOR_LENGTH1;
			break;

         case CMD_STATE_WAIT_FOR_LENGTH1:
            TRACE1("length1 %02x\n", data);
			g_cmd.expectedLength |= (uint16_t)data<<8;
			g_cmd.received	= 0;
			g_cmd.state = CMD_STATE_WAIT_FOR_DATA;
	       	break;

        case CMD_STATE_WAIT_FOR_DATA:
            g_cmd.rxBuf[g_cmd.received] = data;
            g_cmd.received++;

            TRACE1("data:%02x len:%02x/%02x\n", data, g_cmd.received, g_cmd.expectedLength);

            if (g_cmd.received > g_cmd.expectedLength) {
				uint8_t	type = g_cmd.rxBuf[0];

                TRACE1("CMD_processMessage t:%x ", type);
				TRACE1_BUF("",	PRINT_BUF_STYLE_HEX_SIZE_NL, g_cmd.rxBuf+1, g_cmd.received-1);

                CMD_processMessage(i_pContext, type, g_cmd.rxBuf+1, g_cmd.received-1);
                CMD_parseInit();
            }
        break;

        default:
            CMD_parseInit();
    }
}

bool CMD_setStreamContext(CMD_CONTEXT* i_pContext)
{
	if (!i_pContext) {
		return false;
	}

	if (!i_pContext->p_cbSend) {
		return false;
	}

	g_cmd.streamContext.p_cbSend	= i_pContext->p_cbSend;
	g_cmd.streamContext.pArg		= i_pContext->pArg;

	return true;
}

static bool dbgReset(uint8_t argc, char** argv)
{
	CMD_parseInit();
    return true;
}

static bool dbgStream(uint8_t argc, char** argv)
{
	uint32_t	period;

	if (argc < 2) {
		return false;
	}

	period = strtoul(argv[1], NULL, 10);
 
	_streamPeriod(period);

    return true;
}

static bool dbgStatus(uint8_t argc, char** argv)
{
    return true;
}

DEBUG_MENU_START(g_menu)
    DEBUG_MENU_DIR("cmd", NULL)
	    DEBUG_MENU_CMD("status",	NULL,		NULL, dbgStatus)
	    DEBUG_MENU_CMD("reset",		NULL,		NULL, dbgReset)
	    DEBUG_MENU_CMD("stream",	NULL,		NULL, dbgStream)
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

    _init();

	return true;
}