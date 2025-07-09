#pragma once

#include <sys_def.h>

void NVS_init(void);
bool NVS_eraseAll(void);
bool NVS_get(char*namespace, char* key,  char* val, size_t maxSize);
bool NVS_set(char*namespace, char* key,  char* val);
bool NVS_del(char*namespace, char* key);
