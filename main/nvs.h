#pragma once

#include <sys_def.h>

// *INDENT-OFF*
#define NVS_LIST(cmd)	\
	cmd(ssid,		"")	\
	cmd(passwd,		"")	\
	cmd(cert,		"")	\
	cmd(sync_dns,	"")	\
	cmd(sync_port,	"")	\
	cmd(mdns,		"")	\
	cmd(ota_url,	"")	\
	cmd(ota_token,	"")	\
	cmd(ota_updated,"0")\

// *INDENT-ON*

typedef struct {
	char*	pId;
	char*	pDefault;
} nvs_arr_t;

#define NVS_ENUM(id, def)	nvs_id_ ## id,
#define NVS_ARR(id, def)	[nvs_id_ ## id] = {.pId = #id, .pDefault = #def},

typedef enum {
	nvs_id_invalid,
	NVS_LIST(NVS_ENUM)
} nvs_id_t;

#define NVS_KEY_OTA_UPDATED "ota_updated"

void NVS_init(void);
bool NVS_eraseAll(void);
bool NVS_set(nvs_id_t id,  char* val);
bool NVS_get(nvs_id_t id,  char* val);
bool NVS_get_ssid(char* ssid, char* passwd);
bool NVS_set_ssid(char* ssid, char* passwd);
