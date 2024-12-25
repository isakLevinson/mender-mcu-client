
#define DEF_DBG_MODULE	DBG_MODULE_APP

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include "esp_event.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"

#include "soc/soc_caps.h"
#include "driver/mcpwm_timer.h"
#include "driver/mcpwm_oper.h"
#include "driver/mcpwm_cmpr.h"
#include "driver/mcpwm_gen.h"
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "argtable3/argtable3.h"
#include "freertos/task.h"

#include "ctrl.h"
#include "pump.h"
#include "adc.h"
#include "led.h"
#include "cli.h"

static const uint8_t g_valveGpios[] = {
	GPIO_VALVE_0,
	GPIO_VALVE_1,
	GPIO_VALVE_2,
	GPIO_VALVE_3,
};

#define VALVE_COUNT     (sizeof(g_valveGpios)/sizeof(g_valveGpios[0]))

typedef enum {
	PRESS_STATE_IDLE        = 0,
	PRESS_STATE_INFLATE     = 1,
	PRESS_STATE_DEFLATE     = -1,
} PRESS_STATE;

static struct {
	struct {
		uint16_t    pmpValveDelay;
		uint16_t    pmpValveDelayGainPercent;
		int8_t      histeresisH2;
		int8_t      histeresisH1;
		int8_t      histeresisL1;
		int8_t      histeresisL2;
		int8_t      zeroTurnOff;
		uint16_t    zeroValveOpenDelay;

	} cfg;

	bool    loopActive;

	struct {
		int16_t press;
		int16_t target;
		bool    deflateDone;
		PRESS_STATE  pressurizeState;
		bool    valveStatus;
		bool    pumpStatus;
		bool    valveDelay;
		int32_t valveTime;
		bool    valveZeroDelay;
		int32_t valveZeroTime;
	} channels[4];
} g_app = {
	.cfg = {
		.pmpValveDelay  = 100,
		.pmpValveDelayGainPercent  = 100,
		.histeresisH2 = 10,
		.histeresisH1 = 5,
		.histeresisL1 = -5,
		.histeresisL2 = -10,
		.zeroTurnOff   = 10,
		.zeroValveOpenDelay = 5000,
	},
	.loopActive = true,
};

bool _valveOn(uint8_t v, bool on)
{
	if (v >= VALVE_COUNT) {
		return false;
	}

	gpio_set_level(g_valveGpios[v], on);
	g_app.channels[v].valveStatus = on;

	return true;
}

bool _pumpOn(uint8_t v, bool on)
{
	if (v >= 4) {
		return false;
	}
	PMP_on(v, on);
	g_app.channels[v].pumpStatus = on;

	return true;
}

bool CTRL_setPump(uint8_t n, bool on)
{
	bool    ret;

	if (g_app.loopActive) {
		return false;
	}

	ret = _pumpOn(n, on);
	return ret;
}

bool CTRL_setValve(uint8_t n, bool on)
{
	bool    ret;

	if (g_app.loopActive) {
		return false;
	}

	ret = _valveOn(n, on);
	return ret;
}

static void _pressurize(uint8_t ch, PRESS_STATE dir)
{
	uint32_t    valveDelay;

	if (g_app.channels[ch].pressurizeState == dir) {
		return;
	}

	switch (dir) {
		case PRESS_STATE_IDLE:
			g_app.channels[ch].valveDelay = false;
			_pumpOn(ch, 0);
			_valveOn(ch, 0);
			break;

		case PRESS_STATE_INFLATE:
			_pumpOn(ch, 1);
			g_app.channels[ch].valveDelay = true;
			valveDelay = g_app.cfg.pmpValveDelay + g_app.cfg.pmpValveDelayGainPercent * g_app.channels[ch].press / 100;
			INFO("valveDelay[%d]=%d\n", ch, valveDelay);
			g_app.channels[ch].valveTime  = TIME_get32() + valveDelay;
			break;

		case PRESS_STATE_DEFLATE:
			g_app.channels[ch].valveDelay = false;
			_pumpOn(ch, 0);
			_valveOn(ch, 1);
			break;

		default:
	}
	g_app.channels[ch].pressurizeState = dir;
}

static void _task(void* arg)
{
	uint8_t i;
	int32_t t;
	int32_t timeTrace = TIME_get32();

	while (true) {
		int16_t     pressure[4];

		vTaskDelay(10);

		t = TIME_get32();
		ADC_getPressure(pressure);

		for (i = 0; i < 4; i++) {
			g_app.channels[i].press = pressure[i];

			if (g_app.channels[i].valveDelay) {
				if (t >= g_app.channels[i].valveTime) {
					g_app.channels[i].valveDelay = false;
					_valveOn(i, 1);
				}
			}

			if (g_app.channels[i].valveZeroDelay) {
				if (t >= g_app.channels[i].valveZeroTime) {
					g_app.channels[i].valveZeroDelay = false;
					g_app.channels[i].deflateDone = true;
					_pressurize(i, PRESS_STATE_IDLE);
				}
			}
		}

		if (!g_app.loopActive) {
			continue;
		}

		if (t - timeTrace < 100) {
			continue;
		}

		timeTrace += 100;
		TRACE("press: %3d %3d %3d %3d %2d %2d %2d %2d\n",
		    g_app.channels[0].press, g_app.channels[1].press, g_app.channels[2].press, g_app.channels[3].press,
		    g_app.channels[0].pressurizeState, g_app.channels[1].pressurizeState, g_app.channels[2].pressurizeState, g_app.channels[3].pressurizeState);

		for (i = 0; i < 4; i++) {
			g_app.channels[i].press = pressure[i];

			int delta = g_app.channels[i].press - g_app.channels[i].target;

			switch (g_app.channels[i].pressurizeState) {
				case PRESS_STATE_INFLATE:
					if (delta >= g_app.cfg.histeresisH1) {
						_pressurize(i, PRESS_STATE_IDLE);
						g_app.channels[i].deflateDone = true;
					}
					break;

				case PRESS_STATE_DEFLATE:
					if (delta <= g_app.cfg.histeresisL1) {
						_pressurize(i, PRESS_STATE_IDLE);
						//                        if (!g_app.channels[i].target) {
						g_app.channels[i].deflateDone = true;
						//                        }
					}

					if (!g_app.channels[i].valveZeroDelay) {
						if (g_app.channels[i].press <= g_app.cfg.zeroTurnOff) {
							g_app.channels[i].valveZeroDelay = true;
							g_app.channels[i].valveZeroTime = t + g_app.cfg.zeroValveOpenDelay;
						}
					}
					break;

				case PRESS_STATE_IDLE:
					if ((delta > g_app.cfg.histeresisH2)) {
						if (!g_app.channels[i].deflateDone) {
							_pressurize(i, PRESS_STATE_DEFLATE);
						}
					}
					if (delta < g_app.cfg.histeresisL2) {
						_pressurize(i, PRESS_STATE_INFLATE);
					}
					break;
			}
		}
	}
}

static void _init(void)
{
	int i;
	int ret;

	for (i = 0; i < VALVE_COUNT; i++) {
		gpio_set_direction(g_valveGpios[i], GPIO_MODE_OUTPUT);
		gpio_set_level(g_valveGpios[i], 0);
	}

	gpio_set_direction(GPIO_PIEZO_CTRL, GPIO_MODE_OUTPUT);
	gpio_set_level(GPIO_PIEZO_CTRL, 0);

	ret = xTaskCreate(_task, "app", 8192, NULL, 3, NULL);
	if (ret != pdPASS) {
		ERROR("create task %s failed\n", "app");
		return;
	}
}

static void _clearFsm(uint8_t ch)
{
	g_app.channels[ch].deflateDone      = false;
	g_app.channels[ch].valveDelay       = false;
	g_app.channels[ch].valveZeroDelay   = false;
	_pressurize(ch, PRESS_STATE_IDLE);
}

bool CTRL_loopEnable(bool on)
{
	uint32_t    i;

	for (i = 0; i < 4; i++) {
		_clearFsm(i);
	}

	g_app.loopActive = on;
	return true;
}

bool CTRL_setTarget(uint16_t* pPressure)
{
	uint8_t i;

	INFO("CTRL_setTarget %d %d %d %d\n", pPressure[0], pPressure[1], pPressure[2], pPressure[3]);

	for (i = 0; i < 4; i++) {
		if (pPressure[i] == g_app.channels[i].target) {
			continue;
		}
		g_app.channels[i].target = pPressure[i];
		_clearFsm(i);
	}

	return true;
}

bool CTRL_getPressure(int16_t* pPressure)
{
	int i;

	for (i = 0; i < 4; i++) {
		pPressure[i] = g_app.channels[i].press;
	}

	return true;
}

bool CTRL_getValves(bool* pValves)
{
	int i;

	for (i = 0; i < 4; i++) {
		pValves[i] = g_app.channels[i].valveStatus;
	}

	return true;
}

bool CTRL_getPump(bool* pPumpsOn)
{
	int i;

	for (i = 0; i < 4; i++) {
		pPumpsOn[i] = g_app.channels[i].pumpStatus;
	}

	return true;
}

static bool dbgValve(uint8_t argc, char** argv)
{

	uint8_t v;
	bool    on;

	if (argc < 3) {
		return false;
	}

	v = strtoul(argv[1], NULL, 10);
	on = strtoul(argv[2], NULL, 10);

	_valveOn(v, on);

	return true;
}

static bool dbgGpio(uint8_t argc, char** argv)
{
	uint8_t gpio;
	char    val;

	if (argc < 3) {
		return false;
	}

	gpio = strtoul(argv[1], NULL, 10);
	val = argv[2][0];

	switch (val) {
		case '0':
			gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
			gpio_set_level(gpio, 0);
			break;

		case '1':
			gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
			gpio_set_level(gpio, 1);
			break;

		case 'i':
			gpio_set_direction(gpio, GPIO_MODE_INPUT);
			break;

		default:
	}

	return true;
}

static bool dbgCuff(uint8_t argc, char** argv)
{
	bool    ret;
	uint8_t cuff;
	int8_t  op;
	char    c;
	int16_t press[4];

	if (argc < 3)  {
		return false;
	}

	cuff = strtol(argv[1], NULL, 10);
	op = strtol(argv[2], NULL, 10);

	_pressurize(cuff, op);

	do {
		ADC_getPressure(press);
		INFO("%3d %3d %3d %3d\n", press[0], press[1], press[2], press[3]);

		vTaskDelay(100);
		ret = CLI_getc(&c);
	} while (!ret);

	return true;
}

static bool dbgTarget(uint8_t argc, char** argv)
{
	uint16_t   target[4];
	if (argc == 2) {
		target[0] = strtol(argv[1], NULL, 10);
		target[1] = strtol(argv[1], NULL, 10);
		target[2] = strtol(argv[1], NULL, 10);
		target[3] = strtol(argv[1], NULL, 10);
		CTRL_setTarget(target);

		return true;
	}

	if (argc < 5) {
		return false;
	}

	target[0] = strtol(argv[1], NULL, 10);
	target[1] = strtol(argv[2], NULL, 10);
	target[2] = strtol(argv[3], NULL, 10);
	target[3] = strtol(argv[4], NULL, 10);
	CTRL_setTarget(target);

	return true;
}

static bool dbgLoopEnable(uint8_t argc, char** argv)
{
	bool    on;

	if (argc < 2) {
		CTRL_loopEnable(true);
		return true;
	}

	on = strtol(argv[1], NULL, 10);
	CTRL_loopEnable(on);

	return true;
}

static bool dbgPiezoCtrl(uint8_t argc, char** argv)
{
	bool    on;

	if (argc < 2) {
		return false;
	}

	on = strtol(argv[1], NULL, 10);
	gpio_set_level(GPIO_PIEZO_CTRL, on);

	return true;
}

static bool dbgStatus(uint8_t argc, char** argv)
{
	PRINT("target:     %3d %3d %3d %3d\n", g_app.channels[0].target, g_app.channels[1].target, g_app.channels[2].target, g_app.channels[3].target);
	PRINT("press:      %3d %3d %3d %3d\n", g_app.channels[0].press, g_app.channels[1].press, g_app.channels[2].press, g_app.channels[3].press);
	PRINT("state:      %3d %3d %3d %3d\n", g_app.channels[0].pressurizeState, g_app.channels[1].pressurizeState, g_app.channels[2].pressurizeState, g_app.channels[3].pressurizeState);
	PRINT("pump:       %3d %3d %3d %3d\n", g_app.channels[0].pumpStatus, g_app.channels[1].pumpStatus, g_app.channels[2].pumpStatus, g_app.channels[3].pumpStatus);
	PRINT("valve:      %3d %3d %3d %3d\n", g_app.channels[0].valveStatus, g_app.channels[1].valveStatus, g_app.channels[2].valveStatus, g_app.channels[3].valveStatus);
	PRINT("zero delay: %3d %3d %3d %3d\n", g_app.channels[0].valveZeroDelay, g_app.channels[0].valveZeroDelay, g_app.channels[2].valveZeroDelay, g_app.channels[3].valveZeroDelay);
	PRINT("done:       %3d %3d %3d %3d\n", g_app.channels[0].deflateDone, g_app.channels[0].deflateDone, g_app.channels[2].deflateDone, g_app.channels[3].deflateDone);
	return true;
}

static bool dbgCfg(uint8_t argc, char** argv)
{
	bool    ret;

// *INDENT-OFF*
	ARGS_ENTRY_BEGIN(args)
		ARGS_ENTRY("h2",		ARGS_TYPE_INT8,	    0,	"histeresis high2",	&g_app.cfg.histeresisH2)
		ARGS_ENTRY("h1",		ARGS_TYPE_INT8,	    0, 	"histeresis high1",	&g_app.cfg.histeresisH1)
		ARGS_ENTRY("l1",		ARGS_TYPE_INT8,	    0,	"histeresis low1",	&g_app.cfg.histeresisL1)
		ARGS_ENTRY("l2",		ARGS_TYPE_INT8,	    0,	"histeresis low2",	&g_app.cfg.histeresisL2)
		ARGS_ENTRY("mt",		ARGS_TYPE_INT8,	    0,	"min turn off",     &g_app.cfg.zeroTurnOff)
		ARGS_ENTRY("pdg",		ARGS_TYPE_UINT16,	0,	"pump delay",   	&g_app.cfg.pmpValveDelayGainPercent)
		ARGS_ENTRY("pd",		ARGS_TYPE_UINT16,	0,	"pump delay",   	&g_app.cfg.pmpValveDelay)
		ARGS_ENTRY("zd",		ARGS_TYPE_UINT16,	0,	"zero delay",   	&g_app.cfg.zeroValveOpenDelay)
	ARGS_ENTRY_END()
// *INDENT-ON*

	ret = ARGS_readValues(argc, argv, args, NULL, NULL);
	if (!ret) {
		return false;
	}

	PRINT("h2: %d\n", g_app.cfg.histeresisH2);
	PRINT("h1: %d\n", g_app.cfg.histeresisH1);
	PRINT("l1: %d\n", g_app.cfg.histeresisL1);
	PRINT("l2: %d\n", g_app.cfg.histeresisL2);
	PRINT("mt: %d\n", g_app.cfg.zeroTurnOff);
	PRINT("pdg:%d\n", g_app.cfg.pmpValveDelayGainPercent);
	PRINT("pd: %d\n", g_app.cfg.pmpValveDelay);
	PRINT("zd: %d\n", g_app.cfg.zeroValveOpenDelay);

	return true;
}

// *INDENT-OFF*
DEBUG_MENU_START(g_menu)
	DEBUG_MENU_DIR("ctrl", NULL)
		DEBUG_MENU_CMD("status",	NULL,		NULL, dbgStatus)
		DEBUG_MENU_CMD("cfg",   	NULL,		NULL, dbgCfg)
		DEBUG_MENU_CMD("valve",		NULL,		NULL, dbgValve)
		DEBUG_MENU_CMD("gpio",		NULL,		NULL, dbgGpio)
		DEBUG_MENU_CMD("cuff",		NULL,		NULL, dbgCuff)
		DEBUG_MENU_CMD("target",    NULL,		NULL, dbgTarget)
		DEBUG_MENU_CMD("loop",    	NULL,		NULL, dbgLoopEnable)
		DEBUG_MENU_CMD("piezoCfg", 	NULL,		NULL, dbgPiezoCtrl)
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*

void CTRL_init(void)
{
	DBG_TREE_add("/",		g_menu);

	_init();
}