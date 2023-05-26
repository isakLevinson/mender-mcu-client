#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void ADS1299_init(void);
bool ADS1299_cmd(uint8_t dev, uint8_t ch, uint8_t cmd);
bool ADS1299_regRd(uint8_t dev, uint8_t ch, uint8_t startReg, uint8_t* regs, uint8_t count);
bool ADS1299_regWr(uint8_t dev, uint8_t ch, uint8_t startReg, uint8_t* regs, uint8_t count);

#ifdef __cplusplus
}
#endif
