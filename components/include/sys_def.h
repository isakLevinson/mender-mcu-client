/*
* sys_def.h
*/

#ifndef SYS_DEF_H_
#define SYS_DEF_H_

#include "conventions.h"
#include <string.h>
#include "memory.h"

#define VERSION     "0.1"
#define _PROMPT     "esp"

#define SIMULATION_MODE     true

#define SOFTWARE_MAJOR_VERSION			7
#define SOFTWARE_MINOR_VERSION			0
#define SOFTWARE_PATCH_VERSION			251
#define HARDWARE_MAJOR_VERSION			5
#define HARDWARE_MINOR_VERSION			0



#define UDP_SERVER_PORT		5000
#define TCP_CMD_PORT		5002
#define UDP_CMD_PORT		5002

#define CMD_INCOMING_MESSAGE_MAX_SIZE	30
#define CMD_OUT_MESSAGES_QUEUE_SIZE		40
#define CMD_OUT_MESSAGE_MAX_LENGTH		50


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


#endif /* SYS_DEF_H_ */
