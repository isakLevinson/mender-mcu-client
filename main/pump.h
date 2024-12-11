#ifndef PUMP_H_
#define PUMP_H_

#include <sys_def.h>

#if CONFIG_BUILD_TYPE_PNU
void PMP_init(void);
bool PMP_on(uint8_t ch, uint32_t val);
#else
#define PMP_init()
#define PMP_on()    false
#endif

#endif // PUMP_H_