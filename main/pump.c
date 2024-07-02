
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
#define SERVO_TIMEBASE_PERIOD        436       // 20000 ticks, 43uS (23KHz)
#define CHANNEL_COUNT   4

static const mcpwm_generator_config_t generator_bridge_config[CHANNEL_COUNT][2] = {
    {{.gen_gpio_num = 41},  {.gen_gpio_num = 40, .flags.invert_pwm = true}},
    {{.gen_gpio_num = 39},  {.gen_gpio_num = 38, .flags.invert_pwm = true}},
    {{.gen_gpio_num = 37},  {.gen_gpio_num = 36, .flags.invert_pwm = true}},
    {{.gen_gpio_num = 35},  {.gen_gpio_num = 34, .flags.invert_pwm = true}},
};

static struct {
    bool    state[4];

    mcpwm_timer_handle_t timer[CHANNEL_COUNT/2];
    mcpwm_oper_handle_t oper_bridge[CHANNEL_COUNT];
    mcpwm_cmpr_handle_t comparator_bridge[CHANNEL_COUNT];
    mcpwm_gen_handle_t generator[CHANNEL_COUNT][2];
} g_pmp = {0};

static void _init(void)
{
    uint8_t     i;

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

    for (i=0; i<CHANNEL_COUNT/2; i++) {
        timer_config.group_id       = i;
        ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &g_pmp.timer[i]));
    }

    for (i=0; i<CHANNEL_COUNT; i++) {
        operator_config.group_id    = i/2;

        ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &g_pmp.oper_bridge[i]));
        ESP_ERROR_CHECK(mcpwm_operator_connect_timer(g_pmp.oper_bridge[i], g_pmp.timer[i/2]));
        ESP_ERROR_CHECK(mcpwm_new_comparator(g_pmp.oper_bridge[i], &comparator_config, &g_pmp.comparator_bridge[i]));
        ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(g_pmp.comparator_bridge[i], SERVO_TIMEBASE_PERIOD/2));
    }

    for (i=0; i<CHANNEL_COUNT/2; i++) {
        ESP_ERROR_CHECK(mcpwm_timer_enable(g_pmp.timer[i]));
        ESP_ERROR_CHECK(mcpwm_timer_start_stop(g_pmp.timer[i], MCPWM_TIMER_START_NO_STOP));
    }
}

bool PMP_on(uint8_t ch, uint32_t val)
{
    esp_err_t err;
    uint8_t i;

    if (g_pmp.state[ch] == val){
        return true;
    }

    g_pmp.state[ch] = val;

    if (!val) {
        mcpwm_del_generator(g_pmp.generator[ch][0]);
        mcpwm_del_generator(g_pmp.generator[ch][1]);
        gpio_set_direction(generator_bridge_config[ch][0].gen_gpio_num, GPIO_MODE_OUTPUT);
        gpio_set_direction(generator_bridge_config[ch][1].gen_gpio_num, GPIO_MODE_OUTPUT);
        gpio_set_level(generator_bridge_config[ch][0].gen_gpio_num, 0);
        gpio_set_level(generator_bridge_config[ch][1].gen_gpio_num, 0);
        return true;
    }

    for (i=0; i<2; i++) {
        err = mcpwm_new_generator(g_pmp.oper_bridge[ch], &generator_bridge_config[ch][i], &g_pmp.generator[ch][i]);
        if (ESP_OK != err) {
            ERROR("mcpwm_new_generator %d\n", err);
        }

        err = mcpwm_generator_set_actions_on_timer_event(g_pmp.generator[ch][i],
                        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH),
                        MCPWM_GEN_TIMER_EVENT_ACTION_END());
        if (ESP_OK != err) {
            ERROR("mcpwm_generator_set_actions_on_timer_event %d\n", err);
        }

        // go low on compare threshold
        err = mcpwm_generator_set_actions_on_compare_event(g_pmp.generator[ch][i],
                        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, g_pmp.comparator_bridge[ch], MCPWM_GEN_ACTION_LOW),
                        MCPWM_GEN_COMPARE_EVENT_ACTION_END());
        if (ESP_OK != err) {
            ERROR("mcpwm_generator_set_actions_on_compare_event %d\n", err);
        }
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

    PMP_on(ch, on);

    return true;
}

DEBUG_MENU_START(g_menu)
    DEBUG_MENU_DIR("pump", NULL)
	    DEBUG_MENU_CMD("status",		NULL,		NULL, dbgStatus)
	    DEBUG_MENU_CMD("on",			NULL,		NULL, dbgPwm)
    DEBUG_MENU_DIR_END
DEBUG_MENU_END


void PMP_init(void)
{
    DBG_TREE_add("/",		g_menu);

    _init();
}