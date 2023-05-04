
#define DEF_DBG_MODULE	DBG_MODULE_APP

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "driver/pulse_cnt.h"
#include "driver/gpio.h"

//#include "unity.h"

#include "esp_timer.h"
#include "esp_event.h"
#include "esp_check.h"

#include "soc/soc_caps.h"
#include "argtable3/argtable3.h"

#include "main.h"
#include "cli.h"
#include "motor.h"
#include "enc.h"
#include "adc.h"

#define TIMER_INTERVAL_US   10000
#define TIMER_INTERVAL_MS   (TIMER_INTERVAL_US / 1000)

#define DEG_FRAC    0x100

#define LOAD_MAX_CURRENT_MA    20000

typedef enum {
    STATE_UNINIT,
    STATE_IDLE,
    STATE_ZERO,
    STATE_RUN,
    STATE_GOTO,
    STATE_STOP,
    STATE_SPEED_LOAD,
} STATE;

static EventGroupHandle_t _event_group;

static struct {
    STATE  state;
    struct {
        int32_t target;
        int32_t loadCurrent; 
        int32_t minSpeed;
        int32_t rpm;
        int32_t maxSpeed;
        int32_t maxCurrent;
        int32_t stopSnapRegion;
        int32_t stopGainPercent;
        int32_t loadSensitivity;
        int32_t pid_p;
    } config;
    int     deg64;
    int     speed;
    int     degEncoder;
    int     averageCurrentMa;
} g_app = {
    .config = {
        .rpm = 10,
        .maxSpeed = 50,
        .maxCurrent = 20000,
        .stopSnapRegion = 5 * DEG_FRAC,
        .stopGainPercent = 20,
        .pid_p = 20,
        .loadSensitivity = 100,
    },
    .state = STATE_UNINIT,
    .speed = 10,
    .averageCurrentMa = 0,
};

static void _periodic_timer_cb(void* arg)
{
//    static int64_t prev;
//    int64_t time_since_boot = esp_timer_get_time();

    xEventGroupSetBits(_event_group, 0x01);

    //TRACE("timer: %lld us (%lld)\n", time_since_boot, time_since_boot-prev);
//    prev = time_since_boot;
}

static int _degAdd(int x, int y, int fraction)
{
    int d;

    d = x + y;
    if (d > 180 * fraction) {
        d -= 360 * fraction;
    }

    if (d < -180 * fraction) {
        d += 360 * fraction;
    }
    return d;
}

static void _funcRun(void)
{
    int     dd = 0;

    dd = _degAdd(g_app.deg64, -g_app.degEncoder, DEG_FRAC);

    g_app.speed = CLIP(dd * g_app.config.pid_p / 1000, -g_app.config.maxSpeed, g_app.config.maxSpeed);

    if ((g_app.speed < g_app.config.maxSpeed) &&
        (g_app.averageCurrentMa > -g_app.config.maxCurrent) ) {
        g_app.deg64 = _degAdd(g_app.deg64, g_app.config.rpm * DEG_FRAC * 360 / 60 / TIMER_INTERVAL_MS / 10, DEG_FRAC);
    }

    MOT_setSpeed(g_app.speed);
}

static void _task(void *arg)
{
    bool    encValid;
    int     degreeToTarget;
    int     currentMa;
    int     count = 0;
    int     dd = 0;
    int     di = 0;
//    int     rpmSpeed = 0;
//    int     currentDelta;

    INFO("APP Ready.\n");

    while(true) {
        int bits = xEventGroupWaitBits(_event_group, 0x01, 1, 1, portMAX_DELAY);
        if (!(bits & 0x01)) {
            continue;
        }

        encValid = ENC_get256(&g_app.degEncoder);
        currentMa = ADC_getCurrent();

        g_app.averageCurrentMa += (currentMa - g_app.averageCurrentMa) / 4;

        degreeToTarget = _degAdd(g_app.config.target, -g_app.degEncoder, DEG_FRAC);

#if 0
        if (0 == (count % (100000 / TIMER_INTERVAL_US))) {
            static int64_t prev;
            int64_t time_since_boot = esp_timer_get_time();

            TRACE("(%6lld) tar=%3d, deg=%3d, d=%4d, rpm-delta:%3d, deg16:%d,%d delta-deg:%d\n",
                time_since_boot-prev,
                g_app.config.target, degree, degreeToTarget, g_app.rpmDelta, deg256, g_app.deg256, deltaDeg);
            //TRACE("loop: %lld us \n", time_since_boot, time_since_boot-prev);
            prev = time_since_boot;
        }
#endif
        switch (g_app.state) {
            case STATE_ZERO:
                if (encValid) {
                    MOT_setSpeed(0);
                    INFO("reached zero\n");
                    g_app.state = STATE_IDLE;
                }
                break;

            case STATE_RUN:
                _funcRun();
                TRACE("RUN: deg=%4d, dd=%4d, speed=%4d, I=%6d\n", g_app.deg64 / DEG_FRAC, dd / DEG_FRAC, g_app.speed, g_app.averageCurrentMa);
                break;

            case STATE_GOTO:
                if ((degreeToTarget < g_app.config.stopSnapRegion) && (degreeToTarget > -g_app.config.stopSnapRegion)) {
                     g_app.state = STATE_STOP;
                     break;
                }

                _funcRun();
                TRACE("GOTO: deg=%4d, dd=%4d, speed=%4d, to-target:%4d, I=%6d\n", g_app.deg64 / DEG_FRAC, dd / DEG_FRAC, g_app.speed, degreeToTarget, g_app.averageCurrentMa);
                break;

            case STATE_STOP:
                g_app.speed = CLIP(degreeToTarget * g_app.config.stopGainPercent / 1000, -g_app.config.maxSpeed, g_app.config.maxSpeed);
                MOT_setSpeed(g_app.speed);
                TRACE("STOP: deg=%4d, dd=%4d, speed=%4d, to-target:%4d, I=%6d\n", g_app.deg64 / DEG_FRAC, dd / DEG_FRAC, g_app.speed, degreeToTarget, g_app.averageCurrentMa);
                break;

            case STATE_SPEED_LOAD:
                di = g_app.averageCurrentMa - g_app.config.loadCurrent;

                g_app.speed += di * g_app.config.loadSensitivity / 100000;
                g_app.speed = CLIP(g_app.speed, g_app.config.minSpeed, g_app.config.maxSpeed);

                MOT_setSpeed(g_app.speed);
                

                if (0 == (count % 64)) {
                    TRACE("LOAD: I=%5d, di=%5d, speed=%3d\n", g_app.averageCurrentMa, di, g_app.speed);
                }

                break;

            default:
        }

        count++;
    }

    vTaskDelete(NULL);
}

esp_timer_handle_t periodic_timer;

static bool _init(void)
{
    int ret;

    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &_periodic_timer_cb,
        .name = "periodic"
    };

    _event_group = xEventGroupCreate();

    ret = xTaskCreate(_task, "app", 4096, NULL, 7, NULL);
    if (ret != pdPASS) {
        //ERROR
        return false;
    }

    ret = esp_timer_create(&periodic_timer_args, &periodic_timer);
    if (ESP_OK != ret) {
        ERROR("esp_timer_create %d\n", ret);
    }

    ret = esp_timer_start_periodic(periodic_timer, TIMER_INTERVAL_US);
    if (ESP_OK != ret) {
        ERROR("esp_timer_start_periodic %d\n", ret);
    }

    return true;
}

bool APP_load(int percent, int minSpeed)
{
    g_app.config.loadCurrent = percent * LOAD_MAX_CURRENT_MA / 100;
    g_app.config.minSpeed = minSpeed;

    g_app.state = STATE_SPEED_LOAD;
    g_app.speed = 1;
    MOT_setSpeed(g_app.speed);

    return true;
}

static bool dbgGoto(uint8_t argc, char** argv)
{
    if (argc < 2) {
        return false;
    }

    //if ((STATE_IDLE != g_app.state) && (STATE_STOP != g_app.state)) {
    //    PRINT("invalid state %d\n", g_app.state);
    //    return true;
    //}

    g_app.config.target = strtol(argv[1], NULL, 10) * DEG_FRAC;

    if (argc >= 3) {
        g_app.config.rpm = strtol(argv[2], NULL, 10);
    }

    g_app.state = STATE_GOTO;

    return true;
}

static bool dbgRun(uint8_t argc, char** argv)
{
    bool ret;
    int degEncoder;
    if (argc >= 2) {
        g_app.config.rpm = strtol(argv[1], NULL, 10);
    }

    ret = ENC_get(&degEncoder);
    if (!ret) {
        ERROR("encoder not ready\n");
        return true;
    }

    g_app.deg64 = degEncoder * DEG_FRAC;
    g_app.state = STATE_RUN;

    return true;
}

static bool dbgZero(uint8_t argc, char** argv)
{
    if (argc >= 2) {
        g_app.speed = strtol(argv[1], NULL, 10);
    }

    g_app.state = STATE_ZERO;

    MOT_setSpeed(g_app.speed);

    return true;
}

static bool dbgLoad(uint8_t argc, char** argv)
{
    int percent;
    int minSpeed = g_app.config.minSpeed;

    if (argc < 2) {
        return false;
    }

    percent = strtol(argv[1], NULL, 10);

    if (argc >= 3) {
        minSpeed = strtol(argv[2], NULL, 10);
    }

    if (argc >= 4) {
        g_app.config.rpm = strtol(argv[3], NULL, 10);
    }

    APP_load(percent, minSpeed);

    return true;
}

static bool dbgStop(uint8_t argc, char** argv)
{
    g_app.state = STATE_IDLE;
    g_app.speed = 0;
    MOT_setSpeed(g_app.speed);

    return true;
}

static bool dbgStatus(uint8_t argc, char** argv)
{
    bool    ret;
    int     deg;

    ret = ENC_get(&deg);

    PRINT("config ----\n");
    PRINT("rpm       : %d\n", g_app.config.rpm);
    PRINT("target    : %d\n", g_app.config.target);
    PRINT("minSpeed  : %d\n", g_app.config.minSpeed);
    PRINT("maxSpeed  : %d\n", g_app.config.maxSpeed);
    PRINT("maxCurrent: %d\n", g_app.config.maxCurrent);
    PRINT("stop snap : %d\n", g_app.config.stopSnapRegion);
    PRINT("stop gain : %d\n", g_app.config.stopGainPercent);
    PRINT("PID P     : %d\n", g_app.config.pid_p);
    PRINT("load I    : %d\n", g_app.config.loadCurrent);
    PRINT("load sns  : %d\n", g_app.config.loadSensitivity);

    PRINT("current ----\n");
    PRINT("deg      : %d\n", g_app.deg64 / DEG_FRAC);
    PRINT("state    : %d\n", g_app.state);
    PRINT("speed    : %d\n", g_app.speed);

    if (ret) {
        PRINT("current: %d (d=%d)\n", deg, ABS(deg - g_app.config.target));
    } else {
        PRINT("current: Uninit\n");
    }
    return true;
}

static bool dbgConfig(uint8_t argc, char** argv)
{
    bool    retVal;

// *INDENT-OFF*
	ARGS_ENTRY_BEGIN(args)
		ARGS_ENTRY("r",		ARGS_TYPE_INT32,	0,	"rpm",	                &g_app.config.rpm)
		ARGS_ENTRY("mins",	ARGS_TYPE_INT32,	0,	"maximum motor speed",	&g_app.config.minSpeed)
		ARGS_ENTRY("maxs",	ARGS_TYPE_INT32,	0,	"maximum motor speed",	&g_app.config.maxSpeed)
		ARGS_ENTRY("maxi",	ARGS_TYPE_INT32,	0,	"maximum motor current",&g_app.config.maxCurrent)
		ARGS_ENTRY("t",     ARGS_TYPE_INT32,	0,	"target",           	&g_app.config.target)
        ARGS_ENTRY("ss",    ARGS_TYPE_INT32,	0,	"stop snap region", 	&g_app.config.stopSnapRegion)
        ARGS_ENTRY("sg",    ARGS_TYPE_INT32,	0,	"stop angle gfain", 	&g_app.config.stopGainPercent)
        ARGS_ENTRY("pp",    ARGS_TYPE_INT32,	0,	"PID P",                &g_app.config.pid_p)
        ARGS_ENTRY("ls",    ARGS_TYPE_INT32,	0,	"load sensitivity",     &g_app.config.loadSensitivity)
	ARGS_ENTRY_END()
// *INDENT-ON*

	retVal = ARGS_readValues(argc, argv, args, "", NULL);
	if (false == retVal) {
		return false;
	}

    return true;
}


DEBUG_MENU_START(g_menu)
    DEBUG_MENU_DIR("app", NULL)
	    DEBUG_MENU_CMD("status",	NULL,       		            NULL, dbgStatus)
	    DEBUG_MENU_CMD("config",	NULL,       		            NULL, dbgConfig)
	    DEBUG_MENU_CMD("zero",  	"[speed]",		                NULL, dbgZero)
	    DEBUG_MENU_CMD("goto",	    "<target> [speed]",	            NULL, dbgGoto)
 	    DEBUG_MENU_CMD("run",	    NULL,	                        NULL, dbgRun)
        DEBUG_MENU_CMD("load",	    "[targetCurrent] [minSpeed]",	NULL, dbgLoad)
        DEBUG_MENU_CMD("stop",	    NULL,	                        NULL, dbgStop)
    DEBUG_MENU_DIR_END
DEBUG_MENU_END


void APP_init(void)
{
    DBG_TREE_add("/", g_menu);

    _init();
}