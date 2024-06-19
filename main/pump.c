
#define DEF_DBG_MODULE	DBG_MODULE_PUMP

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


#define SERVO_TIMEBASE_RESOLUTION_HZ 10000000  // 1MHz, 1us per tick
#define SERVO_TIMEBASE_PERIOD        400    // 20000 ticks, 20ms
#define GENERATOR_COUNT 2

static const mcpwm_generator_config_t generator_bridge_config[] = {
    {.gen_gpio_num = 4},
    {.gen_gpio_num = 5},
    {.gen_gpio_num = 6},
    {.gen_gpio_num = 7},
};

mcpwm_oper_handle_t oper_bridge = NULL;
mcpwm_cmpr_handle_t comparator_bridge = NULL;
mcpwm_gen_handle_t generator_bridge[GENERATOR_COUNT] = {0};

static void _init(void)
{
    esp_err_t   err;
    mcpwm_timer_handle_t timer = NULL;
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = SERVO_TIMEBASE_RESOLUTION_HZ,
        .period_ticks = SERVO_TIMEBASE_PERIOD,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };

    mcpwm_operator_config_t operator_config = {
        .group_id = 0, // operator must be in the same group to the timer
    };

    mcpwm_comparator_config_t comparator_config = {
        .flags.update_cmp_on_tez = true,
    };

    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));
    ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &oper_bridge));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper_bridge, timer));
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper_bridge, &comparator_config, &comparator_bridge));
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator_bridge, 0));

#if 0
    mcpwm_timer_event_callbacks_t   timer_cb = {
        .on_full = _tmrFullCb,
        .on_empty = _tmrEmptyCb,
        .on_stop = _tmrStopCb,
    };

    mcpwm_comparator_event_callbacks_t comparator_cb = {
        .on_reach = _cmpReachCb,
    };

    err = mcpwm_timer_register_event_callbacks(timer, &timer_cb, NULL);
    if (ESP_OK != err) {
        ERROR("mcpwm_timer_register_event_callbacks %d\n", err);
    }

    err = mcpwm_comparator_register_event_callbacks(comparator_bridge, &comparator_cb, NULL);
    if (ESP_OK != err) {
        ERROR("mcpwm_comparator_register_event_callbacks %d\n", err);
    }
#endif

    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));
}

static bool _setPwm(uint8_t ch, uint32_t val)
{
    esp_err_t err;
    uint8_t i;

    if (!val) {
        mcpwm_del_generator(generator_bridge[0]);
        mcpwm_del_generator(generator_bridge[1]);
        gpio_set_direction(generator_bridge_config[ch*2 + 0].gen_gpio_num, GPIO_MODE_OUTPUT);
        gpio_set_direction(generator_bridge_config[ch*2 + 1].gen_gpio_num, GPIO_MODE_OUTPUT);
        gpio_set_level(generator_bridge_config[ch*2 + 0].gen_gpio_num, 0);
        gpio_set_level(generator_bridge_config[ch*2 + 1].gen_gpio_num, 0);
        return true;
    }

    for (i=0; i<2; i++) {
        err = mcpwm_new_generator(oper_bridge, &generator_bridge_config[ch*2 + i], &generator_bridge[i]);
        if (ESP_OK != err) {
            ERROR("mcpwm_new_generator %d\n", err);
        }

        err = mcpwm_generator_set_actions_on_timer_event(generator_bridge[i],
                        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH),
                        MCPWM_GEN_TIMER_EVENT_ACTION_END());
        if (ESP_OK != err) {
            ERROR("mcpwm_generator_set_actions_on_timer_event %d\n", err);
        }

        // go low on compare threshold
        err = mcpwm_generator_set_actions_on_compare_event(generator_bridge[i],
                        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator_bridge, MCPWM_GEN_ACTION_LOW),
                        MCPWM_GEN_COMPARE_EVENT_ACTION_END());
        if (ESP_OK != err) {
            ERROR("mcpwm_generator_set_actions_on_compare_event %d\n", err);
        }
    }

    err =mcpwm_comparator_set_compare_value(comparator_bridge, 50);
    if (ESP_OK != err) {
        ERROR("mcpwm_comparator_set_compare_value %d\n", err);
    }

    return true;
}

static bool dbgStatus(uint8_t argc, char** argv)
{
    return true;
}

static bool dbgPwm(uint8_t argc, char** argv)
{
    uint8_t ch;
    bool    on;

    if (argc < 3) {
        return false;
    }

    ch = strtoul(argv[1], NULL, 10);
    on = strtoul(argv[2], NULL, 10);

    _setPwm(ch, on);

    return true;
}

static bool dbgGpio(uint8_t argc, char** argv)
{
    return true;
}

DEBUG_MENU_START(g_menu)
    DEBUG_MENU_DIR("pump", NULL)
	    DEBUG_MENU_CMD("status",		NULL,		NULL, dbgStatus)
	    DEBUG_MENU_CMD("pwm",			NULL,		NULL, dbgPwm)
	    DEBUG_MENU_CMD("gpio",			NULL,		NULL, dbgGpio)
    DEBUG_MENU_DIR_END
DEBUG_MENU_END


void PMP_init(void)
{
    DBG_TREE_add("/",		g_menu);

    _init();
}