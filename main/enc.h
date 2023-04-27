#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void ENC_init(void);
bool ENC_get(int* o_pDegree);
bool ENC_get16(int* o_pDegree16);

#ifdef __cplusplus
}
#endif
