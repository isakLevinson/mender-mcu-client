
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
#define I2C_MASTER_TIMEOUT_MS       100


#define MAX17049_REG_VCELL	0x02
#define MAX17049_REG_SOC	0x04
#define MAX17049_REG_MODE	0x06

#define MAX_FAIL_COUNT	5

static struct {
	uint8_t	failCount;
} g_fg = {
	.failCount = MAX_FAIL_COUNT,
};

static bool _read(uint8_t reg_addr, void* data, size_t len)
{
	int32_t	err;

	if (!g_fg.failCount) {
		return false;
	}

	err = i2c_master_write_read_device(I2C_MASTER_NUM, MAX17049_ADDR, &reg_addr, 1, data, len, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
	if (ESP_OK != err) {
		ERROR("i2c_master_write_read_device\n");
		ESP_printErr(err);
		g_fg.failCount--;
		if (!g_fg.failCount) {
			ERROR("last attenp. will not try to use FG anymore\n");
		}
		return false;
	}

	g_fg.failCount = MAX_FAIL_COUNT;
	return true;
}

static bool _write(uint8_t reg_addr, void* data, size_t len)
{
	int err;
	uint8_t write_buf[32];
	write_buf[0] = reg_addr;

	if (!g_fg.failCount) {
		return false;
	}

	memcpy(write_buf + 1, data, len);
	err = i2c_master_write_to_device(I2C_MASTER_NUM, MAX17049_ADDR, write_buf, len + 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
	if (ESP_OK != err) {
		ERROR("i2c_master_write_to_device\n");
		ESP_printErr(err);
		g_fg.failCount--;
		if (!g_fg.failCount) {
			ERROR("last attenp. will not try to use FG anymore\n");
		}
		return false;
	}

	g_fg.failCount = MAX_FAIL_COUNT;
	return true;
}

static bool _rdReg(uint8_t reg, uint16_t* val)
{
	bool	ret;
	uint8_t buf[2];

	ret = _read(reg, buf, 2);
	if (!ret) {
		return false;
	}

	*val = ((uint16_t)buf[0]) << 8 | buf[1];

	return true;
}
#if 0
static bool _wrReg(uint8_t reg, uint16_t val)
{
	bool	ret;
	uint8_t	buf[2];

	buf[0] = val >> 8;
	buf[1] = val & 0xff;

	ret = _write(reg, buf, 2);
	if (!ret) {
		return false;
	}

	return true;
}
#endif

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

bool fg_get_soc(uint16_t* o_pVal)
{
	bool	ret;
	uint16_t	val;

	ret = _rdReg(MAX17049_REG_SOC, &val);
	if (!ret) {
		return false;
	}

	*o_pVal = val / 256;
	return true;
}

bool fg_get_vbat(uint16_t* o_pVal)
{
	bool	ret;
	uint16_t	val;

	ret = _rdReg(MAX17049_REG_VCELL, &val);
	if (!ret) {
		return false;
	}

	*o_pVal = (uint32_t)val * 78125 / 1000000 * 2;
	return true;
}

static bool dbgRd(uint8_t argc, char** argv)
{
	bool	ret;
	uint8_t	reg;
	uint8_t	len = 2;
	uint8_t	val[32];

	if (argc < 2) {
		return false;
	}

	if (argc >= 3) {
		len = strtoul(argv[2], NULL, 16);
	}

	reg = strtoul(argv[1], NULL, 16);
	ret = _read(reg, &val, len);
	if (!ret) {
		ERROR("_read failed\n");
		return true;
	}
	INFO_BUF("wss_send packet",	PRINT_BUF_STYLE_HEX_SIZE_NL, val, len);

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
	ret = _write(reg, &val, 1);
	if (!ret) {
		ERROR("_write failed\n");
	}

	return true;
}

static bool dbgStatus(uint8_t argc, char** argv)
{
	bool ret;
	uint16_t	soc;
	uint16_t	vbat;

	ret  = fg_get_soc(&soc);
	ret &= fg_get_vbat(&vbat);

	if (!ret) {
		ERROR("failed\n");
		return true;
	}

	PRINT("vbat : %d mV\n", vbat);
	PRINT("soc  : %d %%\n", soc);

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

