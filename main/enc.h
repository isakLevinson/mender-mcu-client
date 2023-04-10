#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void ENC_init(void);
bool ENC_get(int* o_pDegree);

#ifdef __cplusplus
}
#endif
