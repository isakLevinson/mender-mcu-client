#pragma once

#include <sys_def.h>

#if USE_LED

bool MMI_init(void);

#else

#define MMI_init()	true

#endif