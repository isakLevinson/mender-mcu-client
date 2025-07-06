#pragma once

#include <sys_def.h>

// *INDENT-OFF*
#define FACTORY_LIST(cmd)	\
	cmd(private_key)	\
	cmd(public_key)	\
	cmd(manufacturing_date)	\
	cmd(sn)	\
	cmd(hw_revision)	\
	cmd(model)	\
	cmd(ca_certificate)	\
	cmd(vault_url)	\
	cmd(vault_role)	\
	cmd(vault_secret)	\
// *INDENT-ON*

#define FACTORY_ENUM(id)	factory_id_ ## id,

typedef enum {
   FACTORY_LIST(FACTORY_ENUM)
   factory_id_last,
} factory_id;

void FACTORY_init(void);
bool parseFactoryPartition(void);

bool FACTORY_get(factory_id id, char* val);
bool FACTORY_getByStr(char* idStr, char* val);
