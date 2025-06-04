/*
* sys_def.h
*/

#ifndef SYS_DEF_H_
#define SYS_DEF_H_

#include "conventions.h"
#include <string.h>
#include "memory.h"
#include "sdkconfig.h"

#if CONFIG_BUILD_TYPE_ESP
#define PROMPT     "esp"
#elif CONFIG_BUILD_TYPE_PNU
#define PROMPT     "pnu"
#elif CONFIG_BUILD_TYPE_GSR
#define PROMPT     "gsr"
#elif CONFIG_BUILD_TYPE_EEG
#define PROMPT     "eeg"
#else
#error Build type not defined
#endif

#if CONFIG_BUILD_TYPE_ESP
#define USE_ADC		0
#define USE_LED		1
#define USE_STREAM	1

#elif CONFIG_BUILD_TYPE_PNU
#define USE_ADC		1
#define USE_LED		1
#define USE_STREAM	1

#elif CONFIG_BUILD_TYPE_GSR
#define USE_ADC		1
#define USE_LED		1
#define USE_STREAM	0

#elif CONFIG_BUILD_TYPE_EEG
#define USE_ADC		0
#define USE_LED		0
#define USE_STREAM	1

#endif


#define USE_FLASH_LOG				1
#define USE_FIFO_MEMORY_INTERFACE	1
#define SIMULATION_MODE     true

#define HW_VERSION_MAJOR		1
#define HW_VERSION_MINOR		0
#define HW_VERSION_BUILD		0

#define USE_HTTP		1
#define USE_TLS			1
#define USE_WSS			0
#define USE_REST		0
#define HTTP_UNSECURE	0

#define TLS_CMD_PORT		"1000"
#define TLS_STREAM_PORT		"1001"
#define REST_HANDLER_BASE_URI	"/control/"

#define HTTPD_PORT              8000
#define UDP_SERVER_PORT		    5000
#define UDP_TIME_SERVER_PORT	5001
#define TCP_CMD_PORT		    5002
#define UDP_CMD_PORT		    5002

#define TIME_ZONE_HOURS	3

#define MAX_PRESSURE_LIMIT	400

#define CMD_INCOMING_MESSAGE_MAX_SIZE	256
#define CMD_OUT_MESSAGES_QUEUE_SIZE		40
#define CMD_OUT_MESSAGE_MAX_LENGTH		50

#define BUTTOR_PRESS_TIME_FACTORY_RESET	10000

#define UART_PORT_NUM_CMD  (1)
#define UART_BAUD_CMD      (115200)

#define GPIO_UART_RXD       5
#define GPIO_UART_TXD       6
#define GPIO_BOOT_BUTTON    0

#define GPIO_ADC1_CHAN0          ADC_CHANNEL_0   /* gpio 1*/
#define GPIO_ADC1_CHAN1          ADC_CHANNEL_2   /* gpio 3*/
#define GPIO_ADC1_CHAN2          ADC_CHANNEL_1   /* gpio 2*/
#define GPIO_ADC1_CHAN3          ADC_CHANNEL_3   /* gpio 4*/

#if CONFIG_BUILD_TYPE_ESP
#define GPIO_LED        13
#endif

#if CONFIG_BUILD_TYPE_PNU
#define GPIO_VALVE_0    9
#define GPIO_VALVE_1    10
#define GPIO_VALVE_2    11
#define GPIO_VALVE_3    12

#define GPIO_PWM_00 35
#define GPIO_PWM_01 34
#define GPIO_PWM_10 37
#define GPIO_PWM_11 36
#define GPIO_PWM_20 39
#define GPIO_PWM_21 38
#define GPIO_PWM_30 41
#define GPIO_PWM_31 40

#define GPIO_LED        13
#define GPIO_24EN       14
#define GPIO_12EN       15
#define GPIO_PIEZO_CTRL 16

#define GPIO_SCL_FG		8
#define GPIO_SDA_FG		7

#define BATTERY_THRESHOLD_LOW		30
#define BATTERY_THRESHOLD_CRITICAL	15
#define BATTERY_THRESHOLD_EMPTY		0

#endif

#if CONFIG_BUILD_TYPE_GSR
#define GPIO_LED        13

#define SPI_PIN_NUM_MISO 13
#define SPI_PIN_NUM_MOSI 11
#define SPI_PIN_NUM_CLK  12
#define SPI_PIN_NUM_CS   7

#define SPI_MAX30001_HOST    SPI2_HOST

#endif

#define NVS_NAMESPACE      "cfg"
#define NVS_KEY_SSID       "ssid"
#define NVS_KEY_PASSWD     "passwd"
#define NVS_KEY_CERT       "cert"
#define NVS_KEY_SYNC_DNS   "sync_dns"
#define NVS_KEY_SYNC_PORT  "sync_port"
#define NVS_KEY_MDNS       "mdns"
#define NVS_KEY_MDNS       "mdns"
#define NVS_KEY_OTA_URL    "ota_url"

#define MENU_LOC      
#define DBG_MENU_STORAGE    
#define DBG_MENU_ROOT_STORAGE 

#define MAX_PROMPT_SIZE 16
#define DBGMENU_IN_FLASH            0
#define PROJ_OPT_FEATURE_GEN_DEBUG_MENUS
#define PROJ_OPT_FEATURE_GEN_DEBUG_PRINTS
#define DBG_PRINT_MAX_LINE_SIZE     256
#define DBG_MENU_MAX_LINE_SIZE      512
#define DBG_MENU_MAX_ARGS       128
#define DBG_MENU_MAX_PATH_DEPTH     8
#define DBG_MENU_HISTORY_SIZE     32
#define DBG_PARSE_SPACE_PROTECT_CHAR  0xff
#define DBG_MENU_USE_AUTOCOMPLETE
#define DBG_PRINT_USE_COLORS
#define DBG_PRINT_LINUX_STYLE
#define MAX_NUM_POSSIBLE_ENTRIES    32
#define DEFAULT_TERMINAL_WIDTH      128
#define USE_ARGS_FLOAT

//#define FIFO_DEBUG

#endif /* SYS_DEF_H_ */
