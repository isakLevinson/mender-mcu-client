#pragma once

#include <sys_def.h>

#if USE_LED

bool MMI_init(void);
bool	TLS_isConnected(void);

#else

#define MMI_init()			true
#define TLS_isConnected()	false


#endif