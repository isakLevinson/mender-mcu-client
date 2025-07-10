#pragma once

#include <sys_def.h>

void FACTORY_init(void);
bool FACTORY_get(char* key,  char* val, size_t maxSize);
