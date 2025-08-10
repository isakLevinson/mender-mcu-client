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
	cmd(sn,				null,	locked,		NULL)	\
	cmd(manufact_date,	null,	locked,		NULL)	\
	cmd(hw_revision,	null,	locked,		NULL)	\
	cmd(model,			null,	locked,		NULL)	\
	cmd(ota_updated,	cfg,	locked,		"0")	\
	cmd(ssid,			cfg,	mandatory,	NULL)	\
	cmd(passwd,			cfg,	mandatory,	NULL)	\
	cmd(sync_dns,		cfg,	optional,	NULL)	\
	cmd(sync_port,		cfg,	optional,	NULL)	\
	cmd(mdns,			cfg,	optional,	NULL)	\
	cmd(ota_url,		cfg,	mandatory,	CONFIG_MENDER_SERVER_HOST)	\
	cmd(ota_token,		cfg,	mandatory,	CONFIG_MENDER_SERVER_TENANT_TOKEN)	\
	cmd(cert_pem,		cfg,	optional,	NULL)		\
	cmd(cert_key,		cfg,	optional,	NULL)		\
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
	cfg_status_ok,
	cfg_status_syntax_error,
	cfg_status_missing_param,
	cfg_status_unsupported_param,
	cfg_status_invalid_credentials,
} cfg_status_t;

typedef enum {
	cfg_location_invalid,
	cfg_location_none,
	cfg_location_default,
	cfg_location_factory,
	cfg_location_nvs,
	cfg_location_temporary,
} cfg_location_t;

void		 CFG_init(void);
cfg_status_t CFG_parseWssCommand(char* pStr, size_t size);
bool		 CFG_default(void);

bool CFG_get(cfg_id_t id,  char* val, size_t maxSize);
bool CFG_getEx(cfg_id_t id,  char* val, size_t maxSize, cfg_location_t* location);
bool CFG_set(cfg_id_t id,  char* val);
bool CFG_setByName(char* key,  char* val);

cfg_location_t CFG_getLocation(cfg_id_t id);