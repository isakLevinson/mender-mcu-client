
#define DEF_DBG_MODULE	DBG_MODULE_ADC

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_timer.h"

#include "esp_system.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "main.h"
#include "cli.h"
#include "ads1299.h"
#include "spi.h"

#define CMD_WAKEUP  0x02
#define CMD_STANDBY 0x04
#define CMD_RESET   0x06
#define CMD_START   0x08
#define CMD_STOP    0x0a
#define CMD_RDATAC  x010
#define CMD_SDATAC  x011
#define CMD_RDATA   0x12
#define CMD_RREG    0x20
#define CMD_WREG    0x40


bool ADS1299_regRd(uint8_t dev, uint8_t ch, uint8_t startReg, uint8_t* regs, uint8_t count)
{
    bool    ret;
    uint8_t txBuf[2];

    if (count > 32) {
        ERROR("invalid count %d\n", count);
        return false;
    }

    txBuf[0] = CMD_RREG | startReg;
    txBuf[1] = count - 1;

    ret = SPI_txrx(dev, ch, txBuf, sizeof(txBuf), regs, count);

    return ret;
}

bool ADS1299_regWr(uint8_t dev, uint8_t ch, uint8_t startReg, uint8_t* regs, uint8_t count)
{
    bool    ret;
    uint8_t i;
    uint8_t txBuf[34];

    if (count > 32) {
        ERROR("invalid count %d\n", count);
        return false;
    }

    txBuf[0] = CMD_WREG | startReg;
    txBuf[1] = count - 1;

    for (i=0; i<count;i++) {
        txBuf[2+i] = regs[i];
    }

    ret = SPI_tx( dev, ch, txBuf, count + 2);

    return ret;
}

static void _init(void)
{
}

static bool dbgStatus(uint8_t argc, char** argv)
{
    PRINT("status\n");
    return true;
}

static bool dbgRd(uint8_t argc, char** argv)
{
    bool    ret;

    uint8_t dev = 0;
    uint8_t ch = 0;
    uint8_t reg;
    uint8_t count;
    uint8_t regs[32];

    if (argc < 3) {
        return false;
    }

    reg     = strtoul(argv[1], NULL, 16);
    count   = strtoul(argv[2], NULL, 16);

    ret = ADS1299_regRd(dev, ch, reg, regs, count);

    if (!ret) {
        PRINT("ADS1299_regRd failed\n");
        return true;
    }

    PRINT_BUF(NULL, PRINT_BUF_STYLE_HEX_S_NL, regs, count);

    return true;
}

static bool dbgWr(uint8_t argc, char** argv)
{
    bool    ret;

    uint8_t dev = 0;
    uint8_t ch = 0;
    uint8_t reg;
    uint8_t count = 0;
    uint8_t regs[32];
    uint8_t i;
    

    if (argc < 3) {
        return false;
    }

    reg     = strtoul(argv[1], NULL, 16);
    
    for (i=2; i<argc; i++) {
        regs[count] = strtoul(argv[i], NULL, 16);
        count++;
    }

    ret = ADS1299_regWr(dev, ch, reg, regs, count);

    if (!ret) {
        PRINT("ADS1299_regRd failed\n");
    }

    return true;
}

DEBUG_MENU_START(g_menu)
    DEBUG_MENU_DIR("ads1299", NULL)
	    DEBUG_MENU_CMD("status",	NULL,      NULL, dbgStatus)
	    DEBUG_MENU_CMD("rd",	    NULL,      NULL, dbgRd)
	    DEBUG_MENU_CMD("wr",	    NULL,      NULL, dbgWr)
   DEBUG_MENU_DIR_END
DEBUG_MENU_END


void ADS1299_init(void)
{
    DBG_TREE_add("/", g_menu);

    _init();
}