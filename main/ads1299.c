
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
#define CMD_RDATAC  0x10
#define CMD_SDATAC  0x11
#define CMD_RDATA   0x12
#define CMD_RREG    0x20
#define CMD_WREG    0x40

static const uint8_t _enPins[4] = {47, 48, 21, 26};

static void _initDevice(int dev, int ch)
{
    ADS1299_cmd(dev, ch, CMD_RESET);
    ADS1299_cmd(dev, ch, CMD_SDATAC);
}

static void _init(void)
{
    int i;
 
    for (i=0; i<4; i++) {
        gpio_set_direction(_enPins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(_enPins[i], 1);
    }
}

bool ADS1299_cmd(uint8_t dev, uint8_t ch, uint8_t cmd)
{
    bool    ret;

    ret = SPI_tx( dev, ch, &cmd, 1);

    return ret;
}

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

    ret = SPI_tx(dev, ch, txBuf, count + 2);

    return ret;
}

bool ADS1299_dataRd(uint8_t dev, uint8_t ch, uint8_t* o_pBuf, uint16_t size)
{
    bool ret;

    ret =  ADS1299_cmd(dev, ch, 0x12); // RDATA
    if (!ret) {
        ERROR("dataRd cmd failed\n");
        return false;
    }

    ret = SPI_txrxAsync(dev, ch, NULL, 0, o_pBuf, size);
    if (!ret) {
        ERROR("dataRd data failed\n");
        return false;
    }
    return true;
}

static bool dbgCmd(uint8_t argc, char** argv)
{
    bool    ret;

    uint32_t dev = 0;
    uint32_t ch = 0;
    uint32_t cmd;

    if (argc < 2) {
        return false;
    }

	ARGS_ENTRY_BEGIN(args)
		ARGS_ENTRY("d",		ARGS_TYPE_UINT32,	false, "spi",	        &dev)
		ARGS_ENTRY("c",		ARGS_TYPE_UINT32,	false, "channel",   	&ch)
    	ARGS_ENTRY(NULL,	ARGS_TYPE_UINT32,	false, "command",    	&cmd)
    ARGS_ENTRY_END()

	ret = ARGS_readValues(argc, argv, args, NULL, NULL);
	if (false == ret) {
		return false;
	}

    ret = ADS1299_cmd(dev, ch, cmd);

    if (!ret) {
        PRINT("ADS1299_cmd failed\n");
    }

    return true;
}

static bool dbgRd(uint8_t argc, char** argv)
{
    bool    ret;

    uint32_t dev = 0;
    uint32_t ch = 0;
    uint32_t reg = 0;
    uint32_t count = 1;
    uint8_t regs[32];

	ARGS_ENTRY_BEGIN(args)
		ARGS_ENTRY("d",		ARGS_TYPE_UINT32,	false, "spi",	        &dev)
		ARGS_ENTRY("c",		ARGS_TYPE_UINT32,	false, "channel",   	&ch)
    	ARGS_ENTRY("n",		ARGS_TYPE_UINT32,	false, "count", 	    &count)
    	ARGS_ENTRY(NULL,	ARGS_TYPE_UINT32,	false, "starting reg", 	&reg)
    ARGS_ENTRY_END()

	ret = ARGS_readValues(argc, argv, args, NULL, NULL);
	if (false == ret) {
		return false;
	}

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
    uint8_t regs[32];
    
	ARG_TYPE_ARRAY	arrRegs = {
		.array = regs,
	};

	ARGS_ENTRY_BEGIN(args)
		ARGS_ENTRY("d",		ARGS_TYPE_UINT32,	    false, "spi",	        &dev)
		ARGS_ENTRY("c",		ARGS_TYPE_UINT32,	    false, "channel",   	&ch)
    	ARGS_ENTRY(NULL,	ARGS_TYPE_ARRAY_HEX8,	false, "starting reg", 	&arrRegs)
    ARGS_ENTRY_END()

	ret = ARGS_readValues(argc, argv, args, NULL, NULL);
	if (false == ret) {
		return false;
	}
    
    if (arrRegs.size < 2) {
        PRINT("invalid args\n");
    }

    ret = ADS1299_regWr(dev, ch, regs[0], regs+1, arrRegs.size - 1);

    if (!ret) {
        PRINT("ADS1299_regRd failed\n");
    }

    return true;
}

static bool dbgId(uint8_t argc, char** argv)
{
    bool    ret;

    uint32_t dev = 0;
    uint32_t ch = 0;
    uint8_t  regs[2];

	ARGS_ENTRY_BEGIN(args)
		ARGS_ENTRY("d",		ARGS_TYPE_UINT32,	false, "spi",	    &dev)
		ARGS_ENTRY("c",		ARGS_TYPE_UINT32,	false, "channel",	&ch)
    ARGS_ENTRY_END()

	ret = ARGS_readValues(argc, argv, args, NULL, NULL);
	if (false == ret) {
		return false;
	}

    ret = ADS1299_regRd(dev, ch, 0, regs, 1);

    switch (BITFIELD_GET(regs[0], 2, 2)	) {
        case 3:     PRINT("ADS1299-x\n");   break;
        default:    PRINT("unexpected device %x\n", regs[0]);   break;
    }

    switch (BITFIELD_GET(regs[0], 0, 2)	) {
        case 0:     PRINT("ADS1299-4\n");   break;
        case 1:     PRINT("ADS1299-6\n");   break;
        case 2:     PRINT("ADS1299 (8ch)\n");   break;
        default:    PRINT("unexpected device %x\n", regs[0]);   break;
    }

    if (!ret) {
        PRINT("ADS1299_regRd failed\n");
    }

    return true;
}


static bool dbgInit(uint8_t argc, char** argv)
{
    bool    ret;

    uint32_t dev = 0;
    uint32_t ch = 0;

	ARGS_ENTRY_BEGIN(args)
		ARGS_ENTRY("d",		ARGS_TYPE_UINT32,	false, "spi",	        &dev)
		ARGS_ENTRY("c",		ARGS_TYPE_UINT32,	false, "channel",   	&ch)
    ARGS_ENTRY_END()

	ret = ARGS_readValues(argc, argv, args, NULL, NULL);
	if (false == ret) {
		return false;
	}

    _initDevice(dev, ch);

    return true;
}

DEBUG_MENU_START(g_menu)
    DEBUG_MENU_DIR("ads1299", NULL)
	    DEBUG_MENU_ARG("init",      NULL,      NULL, dbgInit)
	    DEBUG_MENU_ARG("id",        NULL,      NULL, dbgId)
	    DEBUG_MENU_ARG("cmd",       NULL,      NULL, dbgCmd)
	    DEBUG_MENU_ARG("r",	        NULL,      NULL, dbgRd)
	    DEBUG_MENU_CMD("w",	        NULL,      NULL, dbgWr)
   DEBUG_MENU_DIR_END
DEBUG_MENU_END


void ADS1299_init(void)
{
    DBG_TREE_add("/", g_menu);

    _init();
}