#pragma once

#include <sys_def.h>

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
