#pragma once

#include <sys_def.h>

#if CONFIG_BUILD_TYPE_PNU

void fg_init(void);
bool fg_get_soc(uint16_t* o_pVal);

#else

#define fg_init
#define fg_get_soc(o_pVal)

#endif
