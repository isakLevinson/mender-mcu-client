#pragma once

#include <sys_def.h>

#if USE_LED
bool LED_init(void);

#else
#define LED_init()	true
#endif
