
#define DEF_DBG_MODULE	DBG_MODULE_APP

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "argtable3/argtable3.h"
#include "cmd_decl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_event.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "soc/soc_caps.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"

#include "main.h"
#include "cli.h"
#include "motor.h"
#include "enc.h"
#include "adc.h"

#define LOAD_MAX_CURRENT_MA    20000

typedef enum {
    STATE_UNINIT,
    STATE_IDLE,
    STATE_ZERO,
    STATE_GOTO,
    STATE_SPEED_LOAD,
} STATE;

static struct {
    STATE  state;
    struct {
        int target;
        int loadCurrent; 
        int minSpeed;
    } config;
    int     speed;
    int     deg16;
} g_app = {
    .state = STATE_UNINIT,
    .speed = 10,
};

static void _task(void *arg)
{
    bool    encValid;
    int     degree;
    int     delta;
    int     currentMa;
    int     averageCurrentMa = 0;
    int     currentDelta;
    int     count = 0;
    int     deg16;
    int     deltaDeg;

    INFO("APP Ready.\n");

    while(true) {
        encValid = ENC_get(&degree);

        delta = g_app.config.target - degree;
        if (delta > 180) {
            delta -= 360;
        }
        if (delta < -180) {
            delta += 360;
        }

        switch (g_app.state) {
            case STATE_ZERO:
                if (encValid) {
                    MOT_setSpeed(0);
                    INFO("reached zero\n");
                    g_app.state = STATE_IDLE;
                }
                break;

            case STATE_GOTO:
                if (delta > 5) {
                    MOT_setSpeed(g_app.speed);
                } else if (delta < -5) {
                    MOT_setSpeed(-g_app.speed);
                } else {
                    MOT_setSpeed(0);
                    g_app.state = STATE_IDLE;
                }
                TRACE("tar=%3d, deg=%3d, d=%d\n", g_app.config.target, degree, delta);
                break;

            case STATE_SPEED_LOAD:
                currentMa = ADC_getCurrent();
                ENC_get16(&deg16);
                deltaDeg = deg16 - g_app.deg16;
                g_app.deg16 = deg16;

                averageCurrentMa += (currentMa - averageCurrentMa) / 4;

                currentDelta = averageCurrentMa - g_app.config.loadCurrent;
                g_app.speed += currentDelta / 1000;

                if (g_app.speed < g_app.config.minSpeed)  {
                    g_app.speed = g_app.config.minSpeed;
                }

                if (g_app.speed < 1)  {
                    g_app.speed = 1;
                }

                if (g_app.speed > 90)  {
                    g_app.speed = 90;
                }

                MOT_setSpeed(g_app.speed);

                count++;
                if (0 == count % 64) {
                    TRACE("deg=%5d, i=%5d, d=%5d, speed=%3d\n", deltaDeg, currentMa, currentDelta, g_app.speed);
                }

                break;

            default:
        }

        vTaskDelay(1);
    }

    vTaskDelete(NULL);
}

static bool _init(void)
{
    int ret;

    ret = xTaskCreate(_task, "app", 4096, NULL, 7, NULL);
    if (ret != pdPASS) {
        //ERROR
        return false;
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

    if (STATE_IDLE != g_app.state) {
        PRINT("invalid state %d\n", g_app.state);
        return true;
    }

    g_app.config.target = strtol(argv[1], NULL, 10);

    if (argc >= 3) {
        g_app.speed = strtol(argv[2], NULL, 10);
    }

    g_app.state = STATE_GOTO;

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

    PRINT("target : %d\n", g_app.config.target);
    PRINT("state  : %d\n", g_app.state);
    PRINT("speed  : %d\n", g_app.speed);

    if (ret) {
        PRINT("current: %d (d=%d)\n", deg, ABS(deg - g_app.config.target));
    } else {
        PRINT("current: Uninit\n");
    }
    return true;
}

DEBUG_MENU_START(g_menu)
    DEBUG_MENU_DIR("app", NULL)
	    DEBUG_MENU_CMD("status",	NULL,       		            NULL, dbgStatus)
	    DEBUG_MENU_CMD("zero",  	"[speed]",		                NULL, dbgZero)
	    DEBUG_MENU_CMD("goto",	    "<target> [speed]",	            NULL, dbgGoto)
        DEBUG_MENU_CMD("load",	    "[targetCurrent] [minSpeed]",	NULL, dbgLoad)
        DEBUG_MENU_CMD("stop",	    NULL,	                        NULL, dbgStop)
    DEBUG_MENU_DIR_END
DEBUG_MENU_END


void APP_init(void)
{
    DBG_TREE_add("/", g_menu);

    _init();
}