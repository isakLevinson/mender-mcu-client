/*
* sys_def.h
*/

#ifndef SYS_DEF_H_
#define SYS_DEF_H_

#include "conventions.h"
#include <string.h>
#include "memory.h"

#define VERSION    "0.1"
#define PROMPT     "eeg"

#define SIMULATION_MODE     true

#define SW_VERSION_MAJOR		0
#define SW_VERSION_MINOR		1
#define SW_VERSION_BUILD		1

#define HW_VERSION_MAJOR		1
#define HW_VERSION_MINOR		0
#define HW_VERSION_BUILD		0


#define SPI_DEVICES         2
#define MODULES_PER_SPI     8

#define UDP_SERVER_PORT		    5000
#define UDP_TIME_SERVER_PORT	5001
#define TCP_CMD_PORT		    5002
#define UDP_CMD_PORT		    5002

#define CMD_INCOMING_MESSAGE_MAX_SIZE	256
#define CMD_OUT_MESSAGES_QUEUE_SIZE		40
#define CMD_OUT_MESSAGE_MAX_LENGTH		50

#define UART_PORT_NUM_CMD  (1)
#define UART_BAUD_CMD      (115200)


#define GPIO_ADC1_CHAN0          ADC_CHANNEL_0   /* gpio 1*/
#define GPIO_ADC1_CHAN1          ADC_CHANNEL_1   /* gpio 2*/
#define GPIO_ADC1_CHAN2          ADC_CHANNEL_2   /* gpio 3*/
#define GPIO_ADC1_CHAN3          ADC_CHANNEL_3   /* gpio 4*/

#define GPIO_UART_RXD       (5)
#define GPIO_UART_TXD       (6)

#define GPIO_VALVE_0    9
#define GPIO_VALVE_1    10
#define GPIO_VALVE_2    11
#define GPIO_VALVE_3    12
#define GPIO_VALVE_4    13

#define GPIO_PWM_00 41
#define GPIO_PWM_01 40
#define GPIO_PWM_10 39
#define GPIO_PWM_11 38
#define GPIO_PWM_20 37
#define GPIO_PWM_21 36
#define GPIO_PWM_30 35
#define GPIO_PWM_31 34



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
