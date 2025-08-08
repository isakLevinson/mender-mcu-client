#pragma once

#include <sys_def.h>

typedef enum {
	config_type_factory,
	config_type_mandatory,
	config_type_optional,
	config_type_locked,
} config_type_t;

// *INDENT-OFF*
#define CFG_LIST(cmd)	\
	cmd(ssid,			cfg,	mandatory,	NULL)	\
	cmd(passwd,			cfg,	mandatory,	NULL)	\
	cmd(sync_dns,		cfg,	optional,	NULL)	\
	cmd(sync_port,		cfg,	optional,	NULL)	\
	cmd(mdns,			cfg,	optional,	NULL)	\
	cmd(ota_url,		cfg,	mandatory,	CONFIG_MENDER_SERVER_HOST)	\
	cmd(ota_token,		cfg,	mandatory,	CONFIG_MENDER_SERVER_TENANT_TOKEN)	\
	cmd(ota_updated,	cfg,	locked,		"0")		\
	cmd(cert_pem,		cfg,	mandatory,	NULL)		\
	cmd(cert_key,		cfg,	mandatory,	NULL)		\
	cmd(sn,				null,	mandatory,	NULL)		\
	cmd(manufact_date,	null,	mandatory,	NULL)		\
	cmd(hw_revision,	null,	mandatory,	NULL)		\
	cmd(model,			null,	mandatory,	NULL)		\
	cmd(ca_pem,			cfg,	optional,	NULL)		\
	cmd(vault_url,		cfg,	optional,	NULL)		\
	cmd(client_cn,		null,	mandatory,	"client")	\	
// *INDENT-ON*

#define CFG_ENUM(id, ns, t, def)	cfg_id_ ## id,

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