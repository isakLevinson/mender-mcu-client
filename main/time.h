#pragma once

#include <sys_def.h>

#ifdef __cplusplus
extern "C" {
#endif

void    TIME_getUpdateTime(int64_t* o_pTime);

// 64 bit time is in uS
void	TIME_get64(int64_t* o_pTime);
int64_t	TIME_set64(int64_t time);

// return time in ms
int32_t	TIME_get32(void);

void TIME_strftime(int64_t epoch, char* format, char* str);

#ifdef __cplusplus
}
#endif
