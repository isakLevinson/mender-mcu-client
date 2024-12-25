#ifndef CTRL_H_
#define CTRL_H_

#include <sys_def.h>

#if CONFIG_BUILD_TYPE_PNU
void CTRL_init(void);
bool CTRL_setTarget(uint16_t* pPressure);
bool CTRL_getPressure(int16_t* pPressure);
bool CTRL_getValves(bool* pValves);
bool CTRL_getPump(bool* pPumpsOn);
bool CTRL_loopEnable(bool on);
bool CTRL_setPump(uint8_t n, bool on);
bool CTRL_setValve(uint8_t n, bool on);

#else
#define CTRL_init()
#define CTRL_setTarget(pPressure)    false
#define CTRL_getPressure(pPressure)  false
#define CTRL_getValves(pValves)      false
#define CTRL_getPump(pPumpsOn)       false
#define CTRL_loopEnable(on)          false
#define CTRL_setPump(n, on)          false
#define CTRL_setValve(n, on)         false
#endif

#endif // CTRL_H_