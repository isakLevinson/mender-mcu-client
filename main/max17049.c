
#define DEF_DBG_MODULE	DBG_MODULE_FG

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/i2c.h"
#include "main.h"

//#define MAX17049_ADDR	(0x36<<1)
#define MAX17049_ADDR	(0x36)

#define I2C_MASTER_NUM              0                           /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_FREQ_HZ          400000                      /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000


#define MAX17049_REG_VCELL	0x02
#define MAX17049_REG_SOC	0x04
#define MAX17049_REG_MODE	0x06

static bool _read(uint8_t reg_addr, uint8_t *data, size_t len)
{
	int32_t	err;
    err = i2c_master_write_read_device(I2C_MASTER_NUM, MAX17049_ADDR, &reg_addr, 1, data, len, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
	if (ESP_OK != err) {
		ERROR("i2c_master_write_read_device\n");
		ESP_printErr(err);
		return false;
	}

	return true;
}

static bool _write(uint8_t reg_addr, uint8_t data)
{
    int err;
    uint8_t write_buf[2] = {reg_addr, data};

    err = i2c_master_write_to_device(I2C_MASTER_NUM, MAX17049_ADDR, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
	if (ESP_OK != err) {
		ERROR("i2c_master_write_to_device\n");
		ESP_printErr(err);
		return false;
	}
	return true;
}

static bool _i2c_master_init(void)
{
	int err;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GPIO_SDA_FG,
        .scl_io_num = GPIO_SCL_FG,
        .sda_pullup_en = GPIO_PULLUP_DISABLE, //The board has a built-in pullup resistor
        .scl_pullup_en = GPIO_PULLUP_DISABLE, //The board has a built-in pullup resistor
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    err = i2c_param_config(I2C_MASTER_NUM, &conf);
	if (ESP_OK != err) {
		ERROR("i2c_param_config %d\n", err);
	}

    err = i2c_driver_install(I2C_MASTER_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
	if (ESP_OK != err) {
		ERROR("i2c_driver_install %d\n", err);
	}

	return true;
}

static void _init(void)
{
	_i2c_master_init();
}

bool fg_get_soc(uint8_t* o_pSoc)
{
	bool	ret;
    uint8_t soc[2];

    ret = _read(MAX17049_REG_SOC, soc, 2);
	if (!ret) {
		return false;
	}

	*o_pSoc = soc[0];
    return true;
}

static bool dbgRd(uint8_t argc, char** argv)
{
	bool	ret;
	uint8_t	reg;
	uint8_t	val;

	if (argc < 2) {
		return false;
	}

	reg    = strtoul(argv[1], NULL, 16);
	ret = _read(reg, &val, 1);
	if (!ret) {
		ERROR("_read failed\n");
		return true;
	}
	PRINT("%02x\n", val);

	return true;
}

static bool dbgWr(uint8_t argc, char** argv)
{
	bool	ret;
	uint8_t	reg;
	uint8_t	val;

	if (argc < 3) {
		return false;
	}

	reg	= strtoul(argv[1], NULL, 16);
	val	= strtoul(argv[2], NULL, 16);
	ret = _write(reg, val);
	if (!ret) {
		ERROR("_write failed\n");
	}

	return true;
}

static bool dbgStatus(uint8_t argc, char** argv)
{
	bool ret;
	uint8_t	soc;

	ret = fg_get_soc(&soc);
	if (!ret) {
		ERROR("fg_get_soc failed\n");
		return true;
	}

	PRINT("soc: %d\n", soc);

	return true;
}


// *INDENT-OFF*
DEBUG_MENU_START(g_menu)
	DEBUG_MENU_DIR("fg",	NULL)
		DEBUG_MENU_CMD("status",	NULL,		NULL, dbgStatus)
		DEBUG_MENU_CMD("r",			NULL,		NULL, dbgRd)
		DEBUG_MENU_CMD("w",			NULL,		NULL, dbgWr)
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*

void fg_init(void)
{
	DBG_TREE_add("/", g_menu);

	_init();
}

