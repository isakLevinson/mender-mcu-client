#pragma once

#include <sys_def.h>

// *INDENT-OFF*
#define CFG_LIST(cmd)	\
	cmd(ssid,			CFG,	NULL)	\
	cmd(passwd,			CFG,	NULL)	\
	cmd(cert,			CFG,	NULL)	\
	cmd(sync_dns,		CFG,	NULL)	\
	cmd(sync_port,		CFG,	NULL)	\
	cmd(mdns,			CFG,	NULL)	\
	cmd(ota_url,		CFG,	CONFIG_MENDER_SERVER_HOST)	\
	cmd(ota_token,		CFG,	CONFIG_MENDER_SERVER_TENANT_TOKEN)	\
	cmd(cert_pem,		CFG,	NULL)	\
	cmd(inter_pem,		CFG,	NULL)	\
	cmd(key_pem,		CFG,	NULL)	\
	cmd(ota_updated,	CFG,	"0")	\
	cmd(sn,				NULL,	NULL)	\
	cmd(manufact_date,	NULL,	NULL)	\
	cmd(hw_revision,	NULL,	NULL)	\
	cmd(model,			NULL,	NULL)	\
	cmd(ca_pem,			CFG,	NULL)	\
	cmd(vault_url,		CFG,	NULL)	\
	cmd(vault_role,		CFG,	NULL)	\
	cmd(vault_secret,	CFG,	NULL)	\
// *INDENT-ON*

#define CFG_ENUM(id, ns, def)	cfg_id_ ## id,

typedef enum {
	cfg_id_invalid,
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
