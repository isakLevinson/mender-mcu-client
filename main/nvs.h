#pragma once

#include <sys_def.h>


#define NVS_KEY_SSID        "ssid"
#define NVS_KEY_PASSWD      "passwd"
#define NVS_KEY_CERT        "cert"
#define NVS_KEY_SYNC_DNS    "sync_dns"
#define NVS_KEY_SYNC_PORT   "sync_port"
#define NVS_KEY_MDNS        "mdns"
#define NVS_KEY_MDNS        "mdns"
#define NVS_KEY_OTA_URL     "ota_url"
#define NVS_KEY_OTA_TOKEN   "ota_tocken"


// *INDENT-OFF*
//		Opcode name					OPCODE	Parameters
#define NVS_LIST(cmd)					\
	cmd(NVS_KEY_SSID,	"")				\
	cmd(NVS_KEY_PASSWD,	"")				\

// *INDENT-ON*


#define NVS_ENUM(id, def)	NVS_ID_ ## id,

typedef enum {
	NVS_ID_INVALID,
	NVS_LIST(NVS_ENUM)
} NVS_ID;

#define NVS_KEY_OTA_UPDATED "ota_updated"















void NVS_init(void);
bool NVS_eraseAll(void);
bool NVS_set(char* key,  char* val);
bool NVS_get(char* key,  char* val);
bool NVS_get_ssid(char* ssid, char* passwd);
bool NVS_set_ssid(char* ssid, char* passwd);
