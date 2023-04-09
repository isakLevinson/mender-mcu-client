
#define DEF_DBG_MODULE	DBG_MODULE_PWM

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_console.h"

#include "argtable3/argtable3.h"
#include "cmd_decl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "iperf.h"
#include "esp_coexist.h"

#include <sys/socket.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"

#include "driver/uart.h"
#include "main.h"
#include "cmd_wifi.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "soc/soc_caps.h"
#include "driver/mcpwm_timer.h"
#include "driver/mcpwm_oper.h"
#include "driver/mcpwm_cmpr.h"
#include "driver/mcpwm_gen.h"
#include "driver/gpio.h"


#undef ESP_LOGI
#define ESP_LOGI(...)

#define MAX_SPEED 95

#define SERVO_TIMEBASE_RESOLUTION_HZ 10000000  // 1MHz, 1us per tick
#define SERVO_TIMEBASE_PERIOD        400    // 20000 ticks, 20ms

#define GENERATOR_COUNT 2

mcpwm_oper_handle_t oper_bridge = NULL;
mcpwm_cmpr_handle_t comparator_bridge = NULL;
mcpwm_gen_handle_t generator_bridge[GENERATOR_COUNT] = {0};

mcpwm_oper_handle_t oper_load = NULL;
mcpwm_cmpr_handle_t comparator_load = NULL;
mcpwm_gen_handle_t generator_load = {0};

static const mcpwm_generator_config_t generator_bridge_config[] = {
    {.gen_gpio_num = 7},
    {.gen_gpio_num = 4},
    {.gen_gpio_num = 5},
    {.gen_gpio_num = 6},
};

static const mcpwm_generator_config_t generator_load_config = {
    .gen_gpio_num = 13,
};


static struct {
    bool        isPwm;
    uint32_t    pwm;
    uint8_t     lowGpio;
    uint8_t     highGpio;
} channels[2] = {0};

static bool _channelSetGpio(int ch, uint8_t lowGpio, uint8_t highGpio)
{
    esp_err_t   err;
    int i;

    if (channels[ch].isPwm) {
        for (i=0; i<2; i++) {
            err = mcpwm_del_generator(generator_bridge[i]);
            if (ESP_OK != err) {
                ERROR("mcpwm_del_generator %d (i=%x)\n", err, i);
                return false;
            }
        }
        channels[ch].isPwm = false;
    }

    channels[ch].highGpio = highGpio;
    channels[ch].lowGpio  = lowGpio;

    gpio_set_direction(generator_bridge_config[ch*2 + 0].gen_gpio_num, GPIO_MODE_OUTPUT);
    gpio_set_direction(generator_bridge_config[ch*2 + 1].gen_gpio_num, GPIO_MODE_OUTPUT);
    gpio_set_level(generator_bridge_config[ch*2 + 0].gen_gpio_num, lowGpio);
    gpio_set_level(generator_bridge_config[ch*2 + 1].gen_gpio_num, highGpio);

    return true;
}

static bool _channelSetPwm(int ch, int pwm)
{
    esp_err_t   err;
    int i;

    if (ch >= 2) {
        ERROR("invalid channel %d\n", ch);
        return false;
    }

    if (channels[1-ch].isPwm) {
        WARN("forcing disabling channel %d\n", 1-ch);
        _channelSetGpio(1-ch, channels[1-ch].lowGpio, channels[1-ch].lowGpio);
    }

    if (!channels[ch].isPwm) {
        for (i=0; i<2; i++) {
            err = mcpwm_new_generator(oper_bridge, &generator_bridge_config[ch*2 + i], &generator_bridge[i]);
            if (ESP_OK != err) {
                ERROR("mcpwm_new_generator %d (i=%x)\n", err, i);
                return false;
            }

            ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_timer_event(generator_bridge[i],
                            MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH),
                            MCPWM_GEN_TIMER_EVENT_ACTION_END()));

            // go low on compare threshold
            ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_compare_event(generator_bridge[i],
                            MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator_bridge, MCPWM_GEN_ACTION_LOW),
                            MCPWM_GEN_COMPARE_EVENT_ACTION_END()));
        }
        channels[ch].isPwm = true;
    }

    if (pwm >= SERVO_TIMEBASE_PERIOD) {
        pwm = SERVO_TIMEBASE_PERIOD - 1;
    }
    INFO("setting pwm to %d\n", pwm);
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator_bridge, pwm));

    return true;
}

static bool _setLoadPwm(int pwm)
{
    if (pwm < 0) {
        pwm = 0;
    }

    if (pwm >= SERVO_TIMEBASE_PERIOD) {
        pwm = SERVO_TIMEBASE_PERIOD - 1;
    }
    INFO("setting load pwm to %d\n", pwm);
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator_load, pwm));
    return true;
}

static bool _init(void)
{
    mcpwm_timer_handle_t timer = NULL;
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = SERVO_TIMEBASE_RESOLUTION_HZ,
        .period_ticks = SERVO_TIMEBASE_PERIOD,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };

    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

    mcpwm_operator_config_t operator_config = {
        .group_id = 0, // operator must be in the same group to the timer
    };

    mcpwm_comparator_config_t comparator_config = {
        .flags.update_cmp_on_tez = true,
    };

    ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &oper_bridge));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper_bridge, timer));
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper_bridge, &comparator_config, &comparator_bridge));
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator_bridge, 0));


    ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &oper_load));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper_load, timer));
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper_load, &comparator_config, &comparator_load));

    ESP_ERROR_CHECK(mcpwm_new_generator(oper_load, &generator_load_config, &generator_load));

    ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_timer_event(generator_load,
                    MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH),
                    MCPWM_GEN_TIMER_EVENT_ACTION_END()));

    // go low on compare threshold
    ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_compare_event(generator_load,
                    MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator_load, MCPWM_GEN_ACTION_LOW),
                    MCPWM_GEN_COMPARE_EVENT_ACTION_END()));

    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));
    return true;
}

void PWM_set(uint8_t ch, uint8_t percent)
{
    uint32_t pwm;
    switch (percent) {
        case 0:
            _channelSetGpio(ch, 0, 0);
            break;
        case 100:
            _channelSetGpio(ch, 1, 1);
            break;
        default:
            pwm = SERVO_TIMEBASE_PERIOD * percent / 100;
            _channelSetPwm(ch, pwm);
    }
}

void PWM_setLoad(uint8_t percent)
{
    int pwm;

    //pwm = SERVO_TIMEBASE_PERIOD * percent / 100;
    pwm = SERVO_TIMEBASE_PERIOD/2 - SERVO_TIMEBASE_PERIOD/2 * percent / 100;

    _channelSetGpio(0, 1, 0);
    _channelSetGpio(1, 0, 0);

    _setLoadPwm(pwm);
}

bool PWM_setSpeed(int speed)
{
    if (speed > MAX_SPEED) {
        speed = MAX_SPEED;
    }

    if (speed < -MAX_SPEED) {
        speed = -MAX_SPEED;
    }

    _setLoadPwm(0);

    if (speed == 0) {
        _channelSetGpio(0, 0, 0);
        _channelSetGpio(1, 0, 0);
    } else if (speed > 0) {
        _channelSetGpio(1, 0, 0);
        PWM_set(0, speed);
    } else if (speed < 0) {
        _channelSetGpio(0, 0, 0);
        PWM_set(1, -speed);
    }

    return true;
}


static bool dbgPwm(uint8_t argc, char** argv)
{
    int gen;
    int percent;
    
    if (argc < 3) {
        return false;
    }

    gen     = strtoul(argv[1], NULL, 10);
    percent = strtoul(argv[2], NULL, 10);

    PWM_set(gen, percent);

    return true;
}

static bool dbgDead(uint8_t argc, char** argv)
{
    int pos;
    int neg;
    mcpwm_dead_time_config_t dt_config = {0};

    if (argc < 3) {
        return false;
    }

    pos = strtoul(argv[1], NULL, 10);
    neg = strtoul(argv[2], NULL, 10);

    dt_config.posedge_delay_ticks = pos;
    dt_config.negedge_delay_ticks = 0;

    ESP_ERROR_CHECK(mcpwm_generator_set_dead_time(generator_bridge[0], generator_bridge[0], &dt_config));

    dt_config.posedge_delay_ticks = 0;
    dt_config.negedge_delay_ticks = neg;

    ESP_ERROR_CHECK(mcpwm_generator_set_dead_time(generator_bridge[0], generator_bridge[1], &dt_config));


    return true;
}

static bool dbgGpio(uint8_t argc, char** argv)
{
    int ch;
    int low;
    int high;

    if (argc < 4) {
        return false;
    }

    ch = strtoul(argv[1], NULL, 10);
    low = strtoul(argv[2], NULL, 10);
    high = strtoul(argv[3], NULL, 10);

    _channelSetGpio(ch, low, high);

    return true;
}

static bool dbgLoad(uint8_t argc, char** argv)
{
    int load;

    if (argc < 2) {
        return false;
    }

    load = strtoul(argv[1], NULL, 10);

     PWM_setLoad(load);

    return true;
}


static bool dbgSpeed(uint8_t argc, char** argv)
{
    int speed;

    if (argc < 2) {
        return false;
    }

    speed = strtoul(argv[1], NULL, 10);

     PWM_setSpeed(speed);

    return true;
}

DEBUG_MENU_START(g_menu)
    DEBUG_MENU_DIR("motor", NULL)
	    DEBUG_MENU_CMD("pwm",			NULL,		NULL, dbgPwm)
	    DEBUG_MENU_CMD("gpio",			NULL,		NULL, dbgGpio)
	    DEBUG_MENU_CMD("dead",			NULL,		NULL, dbgDead)
	    DEBUG_MENU_CMD("load",			NULL,		NULL, dbgLoad)
	    DEBUG_MENU_CMD("speed",			NULL,		NULL, dbgSpeed)
    DEBUG_MENU_DIR_END
DEBUG_MENU_END


void MOT_init(void)
{
    DBG_TREE_add("/",		g_menu);

    _init();
}