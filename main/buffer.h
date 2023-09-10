/* Iperf example — declarations of command registration functions.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define MAXIMAL_PROCESSING_BUFFERS_COUNT	50
#define PROCESSING_BUFFER_MAX_SIZE			2200//(2156)

typedef struct {
   uint32_t id;
   uint16_t len;
   uint8_t  buf[PROCESSING_BUFFER_MAX_SIZE];
   uint8_t  cs;
} BUFFER;

bool     BUFFER_init(void);
BUFFER*  BUFFER_getHead(void);
BUFFER*  BUFFER_getTail(void);
bool     BUFFER_push(void);
bool     BUFFER_pop(void);
bool     BUFFER_rewind(uint32_t id);

bool     BUFFERS_addBuf(BUFFER* i_pBuf, void* i_pData, uint16_t size);
bool     BUFFERS_addByte(BUFFER* i_pBuf, uint8_t data);
uint8_t  BUFFERS_getCs(BUFFER* i_pBuf);


#ifdef __cplusplus
}
#endif
