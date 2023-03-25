
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

mcpwm_cmpr_handle_t comparator = NULL;
mcpwm_gen_handle_t generator[2] = {0};

static void _init(void)
{
    mcpwm_timer_handle_t timer = NULL;
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = SERVO_TIMEBASE_RESOLUTION_HZ,
        .period_ticks = SERVO_TIMEBASE_PERIOD,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };

    int i;

    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

    mcpwm_oper_handle_t oper = NULL;
    mcpwm_operator_config_t operator_config = {
        .group_id = 0, // operator must be in the same group to the timer
    };
    ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &oper));

    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper, timer));

    mcpwm_comparator_config_t comparator_config = {
        .flags.update_cmp_on_tez = true,
    };

    ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &comparator_config, &comparator));

    mcpwm_generator_config_t generator_config[2] = {
        {.gen_gpio_num = 4},
        {.gen_gpio_num = 5},
    };

    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &generator_config[0], &generator[0]));
    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &generator_config[1], &generator[1]));

    // set the initial compare value, so that the servo will spin to the center position
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, 0));

    // go high on counter empty
    for (i=0; i<2; i++) {
    }

    ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_timer_event(generator[0],
                    MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH),
                    MCPWM_GEN_TIMER_EVENT_ACTION_END()));

    ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_timer_event(generator[1],
                    MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH),
                    MCPWM_GEN_TIMER_EVENT_ACTION_END()));

    // go low on compare threshold
    ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_compare_event(generator[0],
                    MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator, MCPWM_GEN_ACTION_LOW),
                    MCPWM_GEN_COMPARE_EVENT_ACTION_END()));

    ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_compare_event(generator[1],
                    MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator, MCPWM_GEN_ACTION_LOW),
                    MCPWM_GEN_COMPARE_EVENT_ACTION_END()));


    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));

}

static int _cmd_pwm(int argc, char **argv)
{
    int pwm;
    
    if (argc < 2) {
        return 0;
    }

    pwm = strtoul(argv[1], NULL, 10);

    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, pwm));

    return 0;
}

static int _cmd_deadTime(int argc, char **argv)
{
    //int genIdx;
    mcpwm_dead_time_config_t dt_config = {0};

    if (argc < 3) {
        return 0;
    }

    //genIdx  =   strtoul(argv[1], NULL, 10);
    dt_config.posedge_delay_ticks = strtoul(argv[1], NULL, 10);
    dt_config.negedge_delay_ticks = strtoul(argv[2], NULL, 10);

    ESP_ERROR_CHECK(mcpwm_generator_set_dead_time(generator[0], generator[1], &dt_config));

    return 0;
}


void register_pwm(void)
{
    const esp_console_cmd_t pwm_cmd = {
        .command = "pwm",
        .help = "set pwm value",
        .hint = NULL,
        .func = &_cmd_pwm,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&pwm_cmd) );

    const esp_console_cmd_t dead_cmd = {
        .command = "pwmDeadTime",
        .help = "set pwm dead time",
        .hint = NULL,
        .func = &_cmd_deadTime,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&dead_cmd) );


    _init();
}