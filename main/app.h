#ifndef APP_H_
#define APP_H_

void APP_init(void);
bool APP_setTarget(uint16_t* pPressure);
bool APP_getPressure(int16_t* pPressure);
bool APP_getValves(bool* pValves);
bool APP_getPump(bool* pPumpsOn);
bool APP_loopEnable(bool on);

#endif // APP_H_