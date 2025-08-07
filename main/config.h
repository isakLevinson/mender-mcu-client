#pragma once

#include <sys_def.h>

// *INDENT-OFF*
#define CFG_LIST(cmd)	\
	cmd(ssid,			cfg,	NULL)		\
	cmd(passwd,			cfg,	NULL)		\
	cmd(sync_dns,		cfg,	NULL)		\
	cmd(sync_port,		cfg,	NULL)		\
	cmd(mdns,			cfg,	NULL)		\
	cmd(ota_url,		cfg,	CONFIG_MENDER_SERVER_HOST)	\
	cmd(ota_token,		cfg,	CONFIG_MENDER_SERVER_TENANT_TOKEN)	\
	cmd(cert_pem,		cfg,	NULL)		\
	cmd(cert_key,		cfg,	NULL)		\
	cmd(ota_updated,	cfg,	"0")		\
	cmd(sn,				null,	NULL)		\
	cmd(manufact_date,	null,	NULL)		\
	cmd(hw_revision,	null,	NULL)		\
	cmd(model,			null,	NULL)		\
	cmd(ca_pem,			cfg,	NULL)		\
	cmd(vault_url,		cfg,	NULL)		\
	cmd(vault_role,		cfg,	NULL)		\
	cmd(vault_secret,	cfg,	NULL)		\
	cmd(vault_token,	null,	NULL)		\
	cmd(client_cn,		null,	"client")	\	
// *INDENT-ON*

#define CFG_ENUM(id, ns, def)	cfg_id_ ## id,

typedef enum {
	CFG_LIST(CFG_ENUM)
	cfg_id_last,
} cfg_id_t;

typedef enum {
	PARSE_STATUS_OK,
	PARSE_STATUS_SYNTAX_ERROR,
	PARSE_STATUS_MISSING_PARAM,
	PARSE_STATUS_UNSUPPORTED_PARAM,
	PARSE_STATUS_INVALID_CREDENTIAL,
} PARSE_STATUS;

typedef enum {
	cfg_location_invalid,
	cfg_location_none,
	cfg_location_default,
	cfg_location_factory,
	cfg_location_nvs,
	cfg_location_temporary,
} cfg_location_t;

void		 CFG_init(void);
PARSE_STATUS CFG_parseWssCommand(char* pStr, size_t size);
bool		 CFG_default(void);

bool CFG_get(cfg_id_t id,  char* val, size_t maxSize);
bool CFG_getEx(cfg_id_t id,  char* val, size_t maxSize, cfg_location_t* location);
bool CFG_set(cfg_id_t id,  char* val);
bool CFG_setByName(char* key,  char* val);

cfg_location_t CFG_getLocation(cfg_id_t id);