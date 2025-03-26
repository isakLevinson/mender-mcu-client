#pragma once

#include <sys_def.h>

#if USE_ADC
void    ADC_init(void);
bool    ADC_getPressure(int16_t* pPress);

#else
#define	ADC_init()
#define ADC_getPressure(pPress)	true
#endif
