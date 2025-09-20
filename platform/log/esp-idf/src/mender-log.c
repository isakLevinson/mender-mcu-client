/**
 * @file      mender-g_log.c
 * @brief     Mender logging interface for ESP-IDF platform
 *
 * Copyright joelguittet and mender-mcu-client contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define DEF_DBG_MODULE	DBG_MODULE_MENDER

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include <esp_log.h>
#include "mender-log.h"

static char g_log[8192] = {0};

void mender_log_print(uint8_t level, const char* filename, const char* function, int line, char* format, ...)
{
	(void)function;

	/* Format message */
	va_list args;
	va_start(args, format);
	vsnprintf(g_log, sizeof(g_log), format, args);
	va_end(args);

	/* Switch depending g_log level */
	switch (level) {
		case MENDER_LOG_LEVEL_ERR:
			ERROR("%s(): %s\n", function, g_log);
			break;
		case MENDER_LOG_LEVEL_WRN:
			WARN("%s(): %s\n", function, g_log);
			break;
		case MENDER_LOG_LEVEL_INF:
			INFO("%s(): %s\n", function, g_log);
			break;
		case MENDER_LOG_LEVEL_DBG:
			TRACE("%s(): %s\n", function, g_log);
			break;
		default:
			break;
	}
}
