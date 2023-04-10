
#define DEF_DBG_MODULE	DBG_MODULE_ADC

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
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "iperf.h"
#include "esp_coexist.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "soc/soc_caps.h"
#include "driver/gpio.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"


#include "main.h"
#include "adc.h"
#include "cli.h"


#define SHUNT_RESISTOR_UOHM 200
#define CURRENT_AMP_GAIN    200

#define EXAMPLE_ADC1_CHAN0          ADC_CHANNEL_7 /* gpio8 IMOT */


#define EXAMPLE_ADC2_CHAN0          ADC_CHANNEL_7 /* gpio18 VMOT */
#define EXAMPLE_ADC2_CHAN1          ADC_CHANNEL_4 /* gpio15 MOT+ */
#define EXAMPLE_ADC2_CHAN2          ADC_CHANNEL_5 /* gpio16 MOT- */

adc_oneshot_unit_handle_t adc1_handle;
adc_oneshot_unit_handle_t adc2_handle;
adc_cali_handle_t adc1_cali_handle = NULL;
adc_cali_handle_t adc2_cali_handle = NULL;
bool do_calibration1;
bool do_calibration2;

static bool example_adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        INFO("calibration scheme version is %s", "Curve Fitting\n");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        INFO("Calibration Success\n");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        WARN("eFuse not burnt, skip software calibration\n");
    } else {
        ERROR("Invalid arg or no memory\n");
    }

    return calibrated;
}

#if 0
static void example_adc_calibration_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    INFO("deregister %s calibration scheme", "Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));

#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    INFO("deregister %s calibration scheme", "Line Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
}
#endif

static void _init(void)
{
   //-------------ADC1 Init---------------//
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };

    adc_oneshot_unit_init_cfg_t init_config2 = {
        .unit_id = ADC_UNIT_2,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    //-------------ADC1 Config---------------//
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, EXAMPLE_ADC1_CHAN0, &config));

    //-------------ADC1 Calibration Init---------------//
    do_calibration1 = example_adc_calibration_init(ADC_UNIT_1, ADC_ATTEN_DB_11, &adc1_cali_handle);


   //-------------ADC2 Init---------------//
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config2, &adc2_handle));

    //-------------ADC2 Config---------------//
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc2_handle, EXAMPLE_ADC2_CHAN0, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc2_handle, EXAMPLE_ADC2_CHAN1, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc2_handle, EXAMPLE_ADC2_CHAN2, &config));

    //-------------ADC2 Calibration Init---------------//
    do_calibration2 = example_adc_calibration_init(ADC_UNIT_2, ADC_ATTEN_DB_11, &adc2_cali_handle);
}

static bool dbgStatus(uint8_t argc, char** argv)
{
    bool    ret;
    char    c;
    int val1[3];
    int val2[3];

    int current_ma;
    int voltage[4];

    int delay = 10;

    if (argc >= 2) {
        delay = strtoul(argv[1], NULL, 10);
    }

    do {
        adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN0, &val1[0]);
        adc_oneshot_read(adc2_handle, EXAMPLE_ADC2_CHAN0, &val2[0]);
        adc_oneshot_read(adc2_handle, EXAMPLE_ADC2_CHAN1, &val2[1]);
        adc_oneshot_read(adc2_handle, EXAMPLE_ADC2_CHAN2, &val2[2]);

        PRINT("%6d %6d %6d %6d", val1[0], val2[0], val2[1], val2[2]);
        if (do_calibration1) {
            int shuntVoltage_uv;
            int current_a;
            int current_sign;

            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, val1[0], &voltage[0]));

            shuntVoltage_uv = (1625 - voltage[0]) * 1000 / CURRENT_AMP_GAIN;
            current_ma = shuntVoltage_uv * 1000 / SHUNT_RESISTOR_UOHM;
            if (current_ma >= 0) {
                current_sign = 1;
            } else {
                current_sign = -1;
                current_ma = -current_ma;
            }

            current_a = current_ma / 1000;

            PRINT("0:(%6d) %c%2d.%03dA", voltage[0], (current_sign<0)?'-':' ', current_a, current_ma - current_a*1000);
        }

        if (do_calibration2) {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc2_cali_handle, val2[0], &voltage[0]));
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc2_cali_handle, val2[1], &voltage[1]));
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc2_cali_handle, val2[2], &voltage[2]));
            PRINT("1: (%6d) (%6d) (%6d)", voltage[0], voltage[1], voltage[2]);
        }

        PRINT("\n");

        vTaskDelay(delay);
        ret = CLI_getc(&c);
    } while (!ret);

    PRINT("adc1 0: %d\n", val1[0]);

    PRINT("adc2 0: %d\n", val2[0]);
    PRINT("adc2 1: %d\n", val2[1]);
    PRINT("adc2 2: %d\n", val2[2]);


    //PRINT("ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, EXAMPLE_ADC1_CHAN0, adc_raw[0][0]);
    //if (do_calibration1) {
    //    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, adc_raw[0][0], &voltage[0][0]));
    //    ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, EXAMPLE_ADC1_CHAN0, voltage[0][0]);
    //}


    return true;
}

DEBUG_MENU_START(g_menu)
    DEBUG_MENU_DIR("adc", NULL)
	    DEBUG_MENU_CMD("status",			NULL,		NULL, dbgStatus)
    DEBUG_MENU_DIR_END
DEBUG_MENU_END


void ADC_init(void)
{
    DBG_TREE_add("/",		g_menu);

    _init();
}