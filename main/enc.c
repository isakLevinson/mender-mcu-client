
#define DEF_DBG_MODULE	DBG_MODULE_ENC

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"
#include "esp_event.h"
#include "esp_check.h"

#include "argtable3/argtable3.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "soc/soc_caps.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"

#include "main.h"
#include "cli.h"


#define EXAMPLE_EC11_GPIO_A 1
#define EXAMPLE_EC11_GPIO_B 2
#define EXAMPLE_EC11_GPIO_Z 42

#define ENCODER_COUNTS  (4000)

#define EXAMPLE_PCNT_LOW_LIMIT  (-ENCODER_COUNTS)
#define EXAMPLE_PCNT_HIGH_LIMIT (ENCODER_COUNTS)


static pcnt_unit_handle_t pcnt_unit = NULL;

static struct {
    bool    initialized;
    int     countAtZero;
 } g_enc = {0};

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t)arg;

    if (pcnt_unit) {
        pcnt_unit_get_count(pcnt_unit, &g_enc.countAtZero);
        pcnt_unit_clear_count(pcnt_unit);
        g_enc.initialized = true;
    }
}

static bool _initEncoder(void)
{
    int err;

   INFO("install pcnt unit\n");
    pcnt_unit_config_t unit_config = {
        .high_limit = EXAMPLE_PCNT_HIGH_LIMIT,
        .low_limit = EXAMPLE_PCNT_LOW_LIMIT,
    };
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit));

    INFO("set glitch filter\n");
    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config));

    INFO("install pcnt channels\n");
    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = EXAMPLE_EC11_GPIO_A,
        .level_gpio_num = EXAMPLE_EC11_GPIO_B,
    };
    pcnt_channel_handle_t pcnt_chan_a = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_a_config, &pcnt_chan_a));
    pcnt_chan_config_t chan_b_config = {
        .edge_gpio_num = EXAMPLE_EC11_GPIO_B,
        .level_gpio_num = EXAMPLE_EC11_GPIO_A,
    };
    pcnt_channel_handle_t pcnt_chan_b = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_b_config, &pcnt_chan_b));

    INFO("set edge and level actions for pcnt channels");
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    INFO("enable pcnt unit\n");
    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));
    INFO("clear pcnt unit\n");
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
    INFO("start pcnt unit\n");
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));

    return true;
}

static bool _initGpio(void)
{
    gpio_config_t io_conf = {};

    //interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = EXAMPLE_EC11_GPIO_Z;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 0;

    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(EXAMPLE_EC11_GPIO_Z, gpio_isr_handler, (void*)EXAMPLE_EC11_GPIO_Z);
    gpio_set_intr_type(EXAMPLE_EC11_GPIO_Z, GPIO_INTR_POSEDGE);

    return true;
}

bool ENC_get16(int* o_pDegree16)
{
    int value;

    pcnt_unit_get_count(pcnt_unit, &value);
    if (value < 0) {
        value += ENCODER_COUNTS;
    }

    *o_pDegree16 = value * 360 *16 / ENCODER_COUNTS;

    if (!g_enc.initialized) {
        return false;
    }

    return true;
}

bool ENC_get(int* o_pDegree)
{
    bool    ret;
    int     value;

    ret = ENC_get16(&value);

    *o_pDegree = value / 16;

    return ret;
}


static bool dbgStatus(uint8_t argc, char** argv)
{
    bool    ret;
    int     value;
    int     deg;
    int     delay = 0;
    char    c;

    if (argc >= 2) {
        delay = strtol(argv[1], NULL, 10);
    }

    do {
        ENC_get(&deg);

        pcnt_unit_get_count(pcnt_unit, &value);
        PRINT("%4d zero=%4d, deg=%3d\n", value, g_enc.countAtZero, deg);
        g_enc.countAtZero = 0;

        vTaskDelay(delay);
        ret = CLI_getc(&c);
    } while (!ret && delay);

    return true;
}

static bool dbgClr(uint8_t argc, char** argv)
{
    pcnt_unit_clear_count(pcnt_unit);
    return true;
}

DEBUG_MENU_START(g_menu)
    DEBUG_MENU_DIR("enc", NULL)
	    DEBUG_MENU_CMD("status",	"[delay]",	NULL, dbgStatus)
	    DEBUG_MENU_CMD("clr",		NULL,		NULL, dbgClr)
    DEBUG_MENU_DIR_END
DEBUG_MENU_END


void ENC_init(void)
{
    DBG_TREE_add("/", g_menu);

    _initEncoder();
    _initGpio();
}