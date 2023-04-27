#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void    ADC_init(void);
int     ADC_getCurrent(void);

#ifdef __cplusplus
}
#endif
