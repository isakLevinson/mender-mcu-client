#pragma once

#include <sys_def.h>

#if CONFIG_BUILD_TYPE_PNU

void fg_init(void);

#else

#define fg_init

#endif
