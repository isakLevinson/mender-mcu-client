#pragma once

#include <sys_def.h>

#ifdef __cplusplus
extern "C" {
#endif

void    TIME_getUpdateTime(int64_t* o_pTime);
void	TIME_get64(int64_t* o_pTime);
int32_t	TIME_get32(void);
int64_t	TIME_set64(int64_t time);

#ifdef __cplusplus
}
#endif
