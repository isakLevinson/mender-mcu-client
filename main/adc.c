
#define DEF_DBG_MODULE	DBG_MODULE_ADC

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_netif.h"
#include "esp_event.h"
#include "esp_timer.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_private/adc_private.h"

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include "freertos/FreeRTOS.h"
#include "argtable3/argtable3.h"
#include "freertos/task.h"

#include "soc/soc_caps.h"
#include "driver/gpio.h"

#include "main.h"
#include "adc.h"
#include "cli.h"


#define SHUNT_RESISTOR_UOHM 200
#define CURRENT_AMP_GAIN    100

#define EXAMPLE_ADC1_CHAN0          ADC_CHANNEL_0   /* gpio 1*/
#define EXAMPLE_ADC1_CHAN1          ADC_CHANNEL_1   /* gpio 2*/
#define EXAMPLE_ADC1_CHAN2          ADC_CHANNEL_2   /* gpio 3*/
#define EXAMPLE_ADC1_CHAN3          ADC_CHANNEL_3   /* gpio 4*/


#define MV_TO_NPA500MV(mv)	((mv)*3/2)
#define MV_TO_MMG(mv)		((mv) * 776 / 4000)
#define ADC_MV_TO_MMG(mv)		MV_TO_MMG(MV_TO_NPA500MV((mv)))



adc_oneshot_unit_handle_t adc1_handle;
adc_oneshot_unit_handle_t adc2_handle;
adc_cali_handle_t adc1_cali_handle = NULL;
adc_cali_handle_t adc2_cali_handle = NULL;
bool do_calibration1;

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

bool    ADC_getPressure(int16_t* pPress)
{
    int adcVal = 0;
    int v[4];
    int mmg[4];
    int i;

    adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN0, &adcVal);
    adc_cali_raw_to_voltage(adc1_cali_handle, adcVal, &v[0]);

    adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN1, &adcVal);
    adc_cali_raw_to_voltage(adc1_cali_handle, adcVal, &v[1]);

    adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN2, &adcVal);
    adc_cali_raw_to_voltage(adc1_cali_handle, adcVal, &v[2]);

    adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN3, &adcVal);
    adc_cali_raw_to_voltage(adc1_cali_handle, adcVal, &v[3]);

    for (i=0; i<4; i++) {
       mmg[i]  = MV_TO_MMG(v[i] - 485);
       if (pPress) { 
           pPress[i] = mmg[i];
       }
    }

    //INFO("voltage: %4d %4d %4d %4d     %3d %3d %3d %3d\n", v[0], v[1], v[2], v[3], mmg[0], mmg[1], mmg[2], mmg[3]);

    return true;
}

static void _init(void)
{
   //-------------ADC1 Init---------------//
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11,
    };

    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, EXAMPLE_ADC1_CHAN0, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, EXAMPLE_ADC1_CHAN1, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, EXAMPLE_ADC1_CHAN2, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, EXAMPLE_ADC1_CHAN3, &config));

    do_calibration1 = example_adc_calibration_init(ADC_UNIT_1, ADC_ATTEN_DB_11, &adc1_cali_handle);
}

static bool dbgStatus(uint8_t argc, char** argv)
{
    bool    ret;
    char    c;
    int16_t press[4];

    int delay = 100;
    TickType_t  tick = 0;

    if (argc >= 2) {
        delay = strtoul(argv[1], NULL, 10);
    }

    do {
        if (xTaskGetTickCount() - tick > delay) {
            if (do_calibration1) {
                ADC_getPressure(press);
            }

            tick = xTaskGetTickCount();
        }

        //vTaskDelay(delay);
        ret = CLI_getc(&c);
    } while (!ret);

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