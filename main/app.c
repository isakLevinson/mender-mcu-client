
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
        int32_t stopSnapRegion;
        int32_t stopGainPercent;
        int32_t rpmGainPercent;
    } config;
    int     speed;
    int     deg256;
    int32_t rpmDelta;
} g_app = {
    .config = {
        .rpm = 10,
        .maxSpeed = 50,
        .stopSnapRegion = 5,
        .stopGainPercent = 500,
        .rpmGainPercent = 50,
    },
    .state = STATE_UNINIT,
    .speed = 10,
    .rpmDelta = 0,
};

static void _periodic_timer_cb(void* arg)
{
    static int64_t prev;
    int64_t time_since_boot = esp_timer_get_time();

    xEventGroupSetBits(_event_group, 0x01);

    //TRACE("timer: %lld us (%lld)\n", time_since_boot, time_since_boot-prev);
    prev = time_since_boot;
}


static void _printStatus(void)
{
    
}

static void _task(void *arg)
{
    bool    encValid;
    int     degree;
    int     degreeToTarget;
    int     currentMa;
    int     averageCurrentMa = 0;
    int     count = 0;
    int     deg256;
    int     deltaDeg = 0;
    int     dd = 0;
    int     rpmSpeed = 0;
//    int     currentDelta;

    INFO("APP Ready.\n");

    while(true) {
        int bits = xEventGroupWaitBits(_event_group, 0x01, 1, 1, portMAX_DELAY);
        if (!(bits & 0x01)) {
            continue;
        }

        encValid = ENC_get(&degree);

        degreeToTarget = g_app.config.target - degree;
        if (degreeToTarget > 180) {
            degreeToTarget -= 360;
        }
        if (degreeToTarget < -180) {
            degreeToTarget += 360;
        }

        currentMa = ADC_getCurrent();
        ENC_get256(&deg256);

        dd = deg256 - g_app.deg256;
        g_app.deg256 = deg256;

        if (dd < -180*256) {
            dd += 360*256;
        }

        if (dd > 180*256) {
            dd -= 360*256;
        }

        deltaDeg += (dd - deltaDeg) / 16;

        g_app.rpmDelta = g_app.config.rpm - deltaDeg;

        averageCurrentMa += (currentMa - averageCurrentMa) / 4;

        //currentDelta = averageCurrentMa - g_app.config.loadCurrent;
        //g_app.speed += currentDelta / 1000;

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
                //g_app.speed = CLIP(g_app.speed + g_app.rpmDelta * g_app.config.rpmGainPercent / 100, -g_app.config.maxSpeed, g_app.config.maxSpeed);


                g_app.speed += (g_app.rpmDelta * g_app.config.rpmGainPercent / 100 - g_app.speed) / 16;
                g_app.speed = CLIP(g_app.speed, -g_app.config.maxSpeed, g_app.config.maxSpeed);

                //g_app.speed = CLIP(g_app.rpmDelta * g_app.config.rpmGainPercent / 100, -g_app.config.maxSpeed, g_app.config.maxSpeed);
                MOT_setSpeed(g_app.speed);

                //if (0 == (count % (100000 / TIMER_INTERVAL_US))) {
                    static int64_t prev;
                    int64_t time_since_boot = esp_timer_get_time();

                    TRACE("(%6lld) tar=%3d, deg=%3d, d=%4d, rpm-delta:%3d, deg16:%6d,%6d delta-deg:%d, speed=%4d\n",
                        time_since_boot-prev,
                        g_app.config.target, degree, degreeToTarget, g_app.rpmDelta, deg256, g_app.deg256, deltaDeg, g_app.speed);
                    //TRACE("loop: %lld us \n", time_since_boot, time_since_boot-prev);
                    prev = time_since_boot;
                //}

                break;

            case STATE_GOTO:
                g_app.speed = CLIP(g_app.speed + g_app.rpmDelta * g_app.config.rpmGainPercent / 100, -g_app.config.maxSpeed, g_app.config.maxSpeed);

                if ((degreeToTarget > -g_app.config.stopSnapRegion) && (degreeToTarget < g_app.config.stopSnapRegion)) {
                    g_app.state = STATE_STOP;
                }

                //g_app.speed = CLIP(g_app.speed, -ABS(degreeToTarget), ABS(degreeToTarget));

                MOT_setSpeed(g_app.speed);
                //if (0 == count % 64) {
//                    TRACE("tar=%3d, deg=%3d, d=%d, rpm-delta:%d, delta-deg:%d\n", g_app.config.target, degree, degreeToTarget, g_app.rpmDelta, deltaDeg);
                //}
                break;

            case STATE_STOP:
                g_app.speed = CLIP(degreeToTarget * g_app.config.stopGainPercent / 100, -g_app.config.maxSpeed, g_app.config.maxSpeed);
                MOT_setSpeed(g_app.speed);
                break;

            case STATE_SPEED_LOAD:
                if (g_app.speed < g_app.config.minSpeed)  {
                    g_app.speed = g_app.config.minSpeed;
                }

                if (g_app.speed < rpmSpeed)  {
                    g_app.speed = rpmSpeed;
                }

                if (g_app.speed < 1)  {
                    g_app.speed = 1;
                }

                if (g_app.speed > 90)  {
                    g_app.speed = 90;
                }

                MOT_setSpeed(g_app.speed);

                if (0 == count % 64) {
                    //TRACE("deg=%5d, i=%5d, d=%5d, speed=%3d rpmSpeed=%d\n", deltaDeg, currentMa, currentDelta, g_app.speed, rpmSpeed);
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

    g_app.config.target = strtol(argv[1], NULL, 10);

    if (argc >= 3) {
        g_app.speed = strtol(argv[2], NULL, 10);
    }

    g_app.rpmDelta = 0;
    g_app.state = STATE_GOTO;

    return true;
}

static bool dbgRun(uint8_t argc, char** argv)
{
    if (argc >= 2) {
        g_app.config.rpm = strtol(argv[1], NULL, 10);
    }

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
        g_app.config.rpm
 = strtol(argv[3], NULL, 10);
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
    PRINT("rpm      : %d\n", g_app.config.rpm);
    PRINT("target   : %d\n", g_app.config.target);
    PRINT("minSpeed : %d\n", g_app.config.minSpeed);
    PRINT("maxSpeed : %d\n", g_app.config.maxSpeed);
    PRINT("stop snap: %d\n", g_app.config.stopSnapRegion);
    PRINT("stop gain: %d\n", g_app.config.stopGainPercent);
    PRINT("rpm  gain: %d\n", g_app.config.rpmGainPercent);
    
    PRINT("current ----\n");
    PRINT("rpm : %d\n", g_app.config.rpm);
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
		ARGS_ENTRY("ms",	ARGS_TYPE_INT32,	0,	"maximum motor speed",	&g_app.config.maxSpeed)
		ARGS_ENTRY("t",     ARGS_TYPE_INT32,	0,	"target",           	&g_app.config.target)
        ARGS_ENTRY("ss",    ARGS_TYPE_INT32,	0,	"stop snap region", 	&g_app.config.stopSnapRegion)
        ARGS_ENTRY("sg",    ARGS_TYPE_INT32,	0,	"stop angle gfain", 	&g_app.config.stopGainPercent)
        ARGS_ENTRY("rg",    ARGS_TYPE_INT32,	0,	"rpm gain", 	        &g_app.config.rpmGainPercent)
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