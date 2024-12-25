#pragma once

#include <sys_def.h>

#if CONFIG_BUILD_TYPE_PNU

void fg_init(void);
bool fg_get_soc(uint16_t* o_pVal);
bool fg_get_vbat(uint16_t* o_pVal);

#else

#define fg_init()
#define fg_get_soc(o_pVal)  false
#define fg_get_vbat(o_pVal) false

#endif
