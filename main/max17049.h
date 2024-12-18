#pragma once

#include <sys_def.h>

#if CONFIG_BUILD_TYPE_PNU

void max17049_init(void);

#else

#define max17049_init

#endif
