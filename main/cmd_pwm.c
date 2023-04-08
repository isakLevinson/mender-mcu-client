
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

#define SERVO_TIMEBASE_RESOLUTION_HZ 10000000  // 1MHz, 1us per tick
#define SERVO_TIMEBASE_PERIOD        400    // 20000 ticks, 20ms

#define GENERATOR_COUNT 4

mcpwm_cmpr_handle_t comparator = NULL;
mcpwm_gen_handle_t generator[GENERATOR_COUNT] = {0};

mcpwm_oper_handle_t oper = NULL;

const mcpwm_generator_config_t generator_config[GENERATOR_COUNT] = {
    {.gen_gpio_num = 7},
    {.gen_gpio_num = 4},
    {.gen_gpio_num = 5},
    {.gen_gpio_num = 6},
};

static struct {
    bool        isPwm;
    uint32_t    pwm;
    uint8_t     lowGpio;
    uint8_t     highGpio;
} channels[2] = {0};



static bool _channelDisable(int ch, uint8_t lowGpio, uint8_t highGpio)
{
    esp_err_t   err;
    int i;

    if (channels[ch].isPwm) {
        for (i=0; i<2; i++) {
            err = mcpwm_del_generator(generator[i]);
            if (ESP_OK != err) {
                ERROR("mcpwm_del_generator %d (i=%x)\n", err, i);
                return false;
            }
        }
        channels[ch].isPwm = false;
    }

    channels[ch].highGpio = highGpio;
    channels[ch].lowGpio  = lowGpio;

    gpio_set_direction(generator_config[ch*2 + 0].gen_gpio_num, GPIO_MODE_OUTPUT);
    gpio_set_direction(generator_config[ch*2 + 1].gen_gpio_num, GPIO_MODE_OUTPUT);
    gpio_set_level(generator_config[ch*2 + 0].gen_gpio_num, lowGpio);
    gpio_set_level(generator_config[ch*2 + 1].gen_gpio_num, highGpio);

    return true;
}

static bool _channelEnable(int ch)
{
    esp_err_t   err;
    int i;

    if (ch >= 2) {
        ERROR("invalid channel %d\n", ch);
        return false;
    }

    if (channels[1-ch].isPwm) {
        WARN("forcing disabling channel %d\n", 1-ch);
        _channelDisable(1-ch, channels[1-ch].lowGpio, channels[1-ch].lowGpio);
    }

    if (!channels[ch].isPwm) {
        for (i=0; i<2; i++) {
            err = mcpwm_new_generator(oper, &generator_config[ch*2 + i], &generator[i]);
            if (ESP_OK != err) {
                ERROR("mcpwm_new_generator %d (i=%x)\n", err, i);
                return false;
            }

            ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_timer_event(generator[i],
                            MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH),
                            MCPWM_GEN_TIMER_EVENT_ACTION_END()));

            // go low on compare threshold
            ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_compare_event(generator[i],
                            MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator, MCPWM_GEN_ACTION_LOW),
                            MCPWM_GEN_COMPARE_EVENT_ACTION_END()));
        }
        channels[ch].isPwm = true;
    }
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

    ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &oper));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper, timer));
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &comparator_config, &comparator));

    // set the initial compare value, so that the servo will spin to the center position
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, 0));

    // go high on counter empty
    _channelEnable(0);

    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));
    return true;
}

void _setPwm(uint8_t gen)
{
    ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_timer_event(generator[gen],
                    MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH),
                    MCPWM_GEN_TIMER_EVENT_ACTION_END()));

    ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_compare_event(generator[gen],
                    MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator, MCPWM_GEN_ACTION_LOW),
                    MCPWM_GEN_COMPARE_EVENT_ACTION_END()));

}

void _setZero(uint8_t gen)
{
    ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_timer_event(generator[gen],
                    MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_LOW),
                    MCPWM_GEN_TIMER_EVENT_ACTION_END()));

    ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_compare_event(generator[gen],
                    MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator, MCPWM_GEN_ACTION_LOW),
                    MCPWM_GEN_COMPARE_EVENT_ACTION_END()));

}

void _setOne(uint8_t gen)
{
    ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_timer_event(generator[gen],
                    MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH),
                    MCPWM_GEN_TIMER_EVENT_ACTION_END()));

    ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_compare_event(generator[gen],
                    MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator, MCPWM_GEN_ACTION_HIGH),
                    MCPWM_GEN_COMPARE_EVENT_ACTION_END()));
}


void PWM_set(uint8_t gen, uint8_t percent)
{
    uint32_t pwm;
    switch (percent) {
        case 0:
            _setZero(gen);
            break;
        case 100:
            _setOne(gen);
            break;
        default:
            _setPwm(gen);
            pwm = SERVO_TIMEBASE_PERIOD * percent / 100;
            if (pwm >= SERVO_TIMEBASE_PERIOD) {
                pwm = SERVO_TIMEBASE_PERIOD - 1;
            }
            INFO("setting pwm to %d\n", pwm);
            ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, pwm));
    }
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
//    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, pwm));

    return true;
}

static bool dbgDead(uint8_t argc, char** argv)
{
    //int genIdx;
    mcpwm_dead_time_config_t dt_config = {0};

    if (argc < 3) {
        return false;
    }

    //genIdx  =   strtoul(argv[1], NULL, 10);
    dt_config.posedge_delay_ticks = strtoul(argv[1], NULL, 10);
    dt_config.negedge_delay_ticks = strtoul(argv[2], NULL, 10);

    ESP_ERROR_CHECK(mcpwm_generator_set_dead_time(generator[0], generator[1], &dt_config));
    ESP_ERROR_CHECK(mcpwm_generator_set_dead_time(generator[2], generator[3], &dt_config));

    return true;
}

static bool dbgChannel(uint8_t argc, char** argv)
{
    int ch;
    int en;
    int low;
    int high;

    if (argc < 5) {
        return false;
    }

    ch = strtoul(argv[1], NULL, 10);
    en = strtoul(argv[2], NULL, 10);
    low = strtoul(argv[3], NULL, 10);
    high = strtoul(argv[4], NULL, 10);

    if (en) {
        _channelEnable(ch);
    } else {
        _channelDisable(ch, low, high);
    }

    return true;
}

DEBUG_MENU_START(g_menu)
    DEBUG_MENU_DIR("pwm", NULL)
	    DEBUG_MENU_CMD("pwm",			NULL,		NULL, dbgPwm)
	    DEBUG_MENU_CMD("ch",			NULL,		NULL, dbgChannel)
	    DEBUG_MENU_CMD("dead",			NULL,		NULL, dbgDead)
    DEBUG_MENU_DIR_END
DEBUG_MENU_END


void PWM_init(void)
{
    DBG_TREE_add("/",		g_menu);

    _init();
}