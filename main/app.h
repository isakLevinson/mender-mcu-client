#ifndef APP_H_
#define APP_H_

#include <sys_def.h>

#if CONFIG_BUILD_TYPE_PNU
void APP_init(void);
bool APP_setTarget(uint16_t* pPressure);
bool APP_getPressure(int16_t* pPressure);
bool APP_getValves(bool* pValves);
bool APP_getPump(bool* pPumpsOn);
bool APP_loopEnable(bool on);
bool APP_setPump(uint8_t n, bool on);
bool APP_setValve(uint8_t n, bool on);

#else
#define APP_init()
#define APP_setTarget(pPressure)    false
#define APP_getPressure(pPressure)  false
#define APP_getValves(pValves)      false
#define APP_getPump(pPumpsOn)       false
#define APP_loopEnable(on)          false
#define APP_setPump(n, on)          false
#define APP_setValve(n, on)         false
#endif

#endif // APP_H_