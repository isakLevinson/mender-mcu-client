#pragma once

#if CONFIG_BUILD_TYPE_PNU || CONFIG_BUILD_TYPE_GSR

bool MMI_init(void);

#else

#define MMI_init()	true

#endif