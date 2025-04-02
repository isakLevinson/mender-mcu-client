#pragma once

#include <sys_def.h>

void FACTORY_init(void);
bool parseFactoryPartition(void);
bool FACTORY_factoryGetPrivateKey(char** o_ppStr);
bool FACTORY_factoryGetPublicKey(char** o_ppStr);
bool FACTORY_factoryGetCertificate(char** o_ppStr);
bool FACTORY_factoryGetManufacturingDate(char** o_ppStr);
bool FACTORY_factoryGetSn(char** o_ppStr);
bool FACTORY_factoryGetHwRevision(char** o_ppStr);
bool FACTORY_factoryGetModel(char** o_ppStr);
