#pragma once

#include <sys_def.h>

// *INDENT-OFF*
#define CFG_LIST(cmd)	\
	cmd(ssid,			cfg,	NULL)	\
	cmd(passwd,			cfg,	NULL)	\
	cmd(cert,			cfg,	NULL)	\
	cmd(sync_dns,		cfg,	NULL)	\
	cmd(sync_port,		cfg,	NULL)	\
	cmd(mdns,			cfg,	NULL)	\
	cmd(ota_url,		cfg,	CONFIG_MENDER_SERVER_HOST)	\
	cmd(ota_token,		cfg,	CONFIG_MENDER_SERVER_TENANT_TOKEN)	\
	cmd(cert_pem,		cfg,	NULL)	\
	cmd(inter_pem,		cfg,	NULL)	\
	cmd(key_pem,		cfg,	NULL)	\
	cmd(ota_updated,	cfg,	"0")	\
	cmd(sn,				null,	NULL)	\
	cmd(manufact_date,	null,	NULL)	\
	cmd(hw_revision,	null,	NULL)	\
	cmd(model,			null,	NULL)	\
	cmd(ca_pem,			cfg,	NULL)	\
	cmd(vault_url,		cfg,	NULL)	\
	cmd(vault_role,		cfg,	NULL)	\
	cmd(vault_secret,	cfg,	NULL)	\
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

void		 CFG_init(void);
PARSE_STATUS CFG_parseWssCommand(char* pStr, size_t size);
bool		 CFG_default(void);

bool CFG_get(cfg_id_t id,  char* val, size_t maxSize);
bool CFG_set(cfg_id_t id,  char* val);

