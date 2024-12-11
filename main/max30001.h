#pragma once

#include <sys_def.h>

#if CONFIG_BUILD_TYPE_GSR
void max30001_init(void);
void max30001_write_reg(uint8_t i_reg, uint32_t regVal);
uint32_t max30001_read_reg(uint8_t i_reg);
void max300001_get_status(void);

#else
#define max30001_init()
#define max30001_write_reg(i_reg, regVal)
#define max30001_read_reg(i_reg)          0
#define max300001_get_status()
#endif

