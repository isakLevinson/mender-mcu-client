#pragma once

#include <sys_def.h>

void CFG_init(void);
bool CFG_parseWssCommand(char* pStr, size_t size);
bool CFG_parseFactoryPartition(void);
bool CFG_factoryGetPrivateKey(char** o_ppStr);
bool CFG_factoryGetPublicKey(char** o_ppStr);
bool CFG_factoryGetCertificate(char** o_ppStr);
bool CFG_factoryGetManufacturingDate(char** o_ppStr);
bool CFG_factoryGetSn(char** o_ppStr);
bool CFG_factoryGetHwRevision(char** o_ppStr);
bool CFG_factoryGetModel(char** o_ppStr);
bool CFG_default(void);
