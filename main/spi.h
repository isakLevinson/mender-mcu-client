#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void    SPI_init(void);
bool    SPI_txrxAsync(uint8_t dev, uint8_t ch, void* txBuf, size_t txSize, void* rxBuf, size_t rxSize);
bool    SPI_waitForCompletion(uint8_t dev, int timeout);

bool    SPI_tx(uint8_t dev, uint8_t ch, void* txBuf, size_t txSize);
bool    SPI_txrx(uint8_t dev, uint8_t ch, void* txBuf, size_t txSize, void* rxBuf, size_t rxSize);

#ifdef __cplusplus
}
#endif
