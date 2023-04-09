#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void MOT_init(void);
bool MOT_setSpeed(int speed);
void MOT_setLoad(uint8_t percent);

#ifdef __cplusplus
}
#endif
