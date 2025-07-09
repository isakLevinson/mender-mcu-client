#pragma once

#include <sys_def.h>

// *INDENT-OFF*
#define NVS_LIST(cmd)	\
	cmd(ssid,		NULL)	\
	cmd(passwd,		NULL)	\
	cmd(cert,		NULL)	\
	cmd(sync_dns,	NULL)	\
	cmd(sync_port,	NULL)	\
	cmd(mdns,		NULL)	\
	cmd(ota_url,	CONFIG_MENDER_SERVER_HOST)	\
	cmd(ota_token,	CONFIG_MENDER_SERVER_TENANT_TOKEN)	\
	cmd(cert_pem,	NULL)	\
	cmd(inter_pem,	NULL)	\
	cmd(key_pem,	NULL)	\
	cmd(ota_updated,"0")\
// *INDENT-ON*

#define NVS_ENUM(id, def)	nvs_id_ ## id,

typedef struct {
	char*	key;
	char*	pDefault;
} nvs_arr_t;

typedef enum {
	nvs_id_invalid,
	NVS_LIST(NVS_ENUM)
	nvs_id_last,
} nvs_id_t;

#define NVS_KEY_OTA_UPDATED "ota_updated"

void NVS_init(void);
bool NVS_eraseAll(void);
bool NVS_isValidName(char* pName);
bool NVS_set(nvs_id_t id,  char* val);
bool NVS_get(nvs_id_t id,  char* val, size_t maxSize);
bool NVS_del(nvs_id_t id);
bool NVS_get_ssid(char* ssid, char* passwd, size_t maxSize);
bool NVS_set_ssid(char* ssid, char* passwd);
