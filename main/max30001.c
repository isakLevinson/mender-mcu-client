
#define DEF_DBG_MODULE	DBG_MODULE_MAX30001

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/spi_master.h"
#include "max30001.h"
#include "max30001_private.h"

static struct {
	max30001_registers_t	regs;
	spi_device_handle_t		spi_handle;
	SemaphoreHandle_t		semaphore;

	struct {
		uint32_t rcvCount;
	} ecg;
} g_max;


static uint32_t _readReg(uint8_t i_reg)
{
	uint8_t cmd[4] = {((i_reg << 1) | MAX30001_RREG), 0x00, 0x00, 0x00};
	uint32_t retVal;

	xSemaphoreTake(g_max.semaphore, portMAX_DELAY);

	// When using SPI_TRANS_CS_KEEP_ACTIVE, bus must be locked/acquired
	spi_device_acquire_bus(g_max.spi_handle, portMAX_DELAY);

	spi_transaction_t t;
	memset(&t, 0, sizeof(t));
	t.length = 8 * 4;
	t.tx_buffer = cmd;
	t.flags = SPI_TRANS_USE_RXDATA;
	t.user = (void*)1;

	esp_err_t ret = spi_device_polling_transmit(g_max.spi_handle, &t);
	if (ret != ESP_OK) {
		ERROR("spi_device_polling_transmit %d\n", ret);
	}

	// Release bus
	spi_device_release_bus(g_max.spi_handle);

	xSemaphoreGive(g_max.semaphore);

	retVal = t.rx_data[0] << 24 | t.rx_data[1] << 16 | t.rx_data[2] << 8 | t.rx_data[3];

	return retVal;
}

static void _writeReg(uint8_t i_reg, uint32_t regVal)
{
	xSemaphoreTake(g_max.semaphore, portMAX_DELAY);

	spi_device_acquire_bus(g_max.spi_handle, portMAX_DELAY);

	spi_transaction_t t;
	memset(&t, 0, sizeof(t));
	t.length = 8 * 4;
	t.flags = SPI_TRANS_USE_TXDATA;
	t.tx_data[0] = ((i_reg << 1) | MAX30001_WREG);
	t.tx_data[1] = regVal >> 16;
	t.tx_data[2] = regVal >> 8;
	t.tx_data[3] = regVal;
	t.user = (void*)1;

	esp_err_t ret = spi_device_polling_transmit(g_max.spi_handle, &t);
	if (ret != ESP_OK) {
		ERROR("spi_device_polling_transmit %d\n", ret);
	}

	// Release bus
	spi_device_release_bus(g_max.spi_handle);

	xSemaphoreGive(g_max.semaphore);
}

static void _handleEcg(void)
{
	max30001_ecg_fifo_u val;
	uint8_t count = 0;
	bool	exitLoop = false;
	int32_t	data;

	do {
		val.all = _readReg(MAX3001_REG_ECG_FIFO);

		data = val.bit.data;
		if (val.bit.data & 0x20000) {
			data |= 0xfffc0000;
		}

		switch (val.bit.etag) {
			case 2:
				//TRACE("%2d: %06x %x %x EOF\n", count, val.bit.ecg_data, val.bit.ptag, val.bit.etag);
				TRACE("%2d: %8d %x %x EOF\n", count, data, val.bit.ptag, val.bit.etag);
				g_max.ecg.rcvCount++;
				exitLoop = true;
				break;

			case 3:
				TRACE("%2d: %8d %x %x FAULT EOF\n", count, data, val.bit.ptag, val.bit.etag);
				exitLoop = true;
				break;

			case 6:
				TRACE(" EMPTY\n");
				exitLoop = true;
				break;

			case 7:
				TRACE(" OVERFLOW\n");
				_writeReg(MAX3001_REG_FIFO_RST, 0);
				//count = 0;
				exitLoop = true;
				break;

			default:
				//TRACE("%2d: %06x %x %x\n", count, val.bit.ecg_data, val.bit.ptag, val.bit.etag);
				TRACE("%2d: %8d %x %x\n", count, data, val.bit.ptag, val.bit.etag);
				g_max.ecg.rcvCount++;
		}

		count++;
		if (count > 40) {
			exitLoop = true;
		}
	} while (!exitLoop);

	if (count >= 40) {
		ERROR("failed to detect EOF\n");
	}
}

static void _handleBioz(void)
{
	max30001_bioz_fifo_u val;
	uint8_t count = 0;
	bool	exitLoop = false;
	int32_t	data;

	do {
		val.all = _readReg(MAX3001_REG_BIOZ_FIFO);

		data = val.bit.data;
		if (val.bit.data & 0x080000) {
			data |= 0xfff00000;
		}

		switch (val.bit.btag) {
			case 2:
				//TRACE("%2d: %06x %x %x EOF\n", count, val.bit.ecg_data, val.bit.ptag, val.bit.etag);
				TRACE("%2d: %8d %x EOF\n", count, data, val.bit.btag);
//				g_max.boiz.rcvCount++;
				exitLoop = true;
				break;

			case 3:
				TRACE("%2d: %06x %x FAULF EOF\n", count, val.bit.data, val.bit.btag);
				exitLoop = true;
				break;

			case 6:
				TRACE(" EMPTY\n");
				exitLoop = true;
				break;

			case 7:
				TRACE(" OVERFLOW\n");
				_writeReg(MAX3001_REG_FIFO_RST, 0);
				//count = 0;
				exitLoop = true;
				break;

			default:
				//TRACE("%2d: %06x %x %x\n", count, val.bit.ecg_data, val.bit.ptag, val.bit.etag);
				TRACE("%2d: %8d %x\n", count, data, val.bit.btag);
//				g_max.bioz.rcvCount++;
		}

		count++;
		if (count > 40) {
			exitLoop = true;
		}
	} while (!exitLoop);

	if (count >= 40) {
		ERROR("failed to detect EOF\n");
	}
}

static void _task(void* arg)
{
	while (true) {
		vTaskDelay(10);
		//_handleEcg();
		//_handleBioz();
	}
}

void max30001_start_ecg(void)
{
	uint32_t max30001_timeout = 0;

	g_max.regs.mngrInt.bit.clr_samp = 0x01;
	g_max.regs.mngrInt.bit.bfit     = 0x07;
	g_max.regs.mngrInt.bit.efit     = 0x0F;

	_writeReg(MAX3001_REG_MNGR_INT, g_max.regs.mngrInt.all);

	g_max.regs.cnfgEmux.all = _readReg(MAX3001_REG_CNFG_EMUX);
	g_max.regs.cnfgEmux.bit.openp = 0; // Positive lead enabled
	g_max.regs.cnfgEmux.bit.openn = 0; // Negative lead enabled
	g_max.regs.cnfgEmux.bit.pol = 0;   // Positive polarity
	g_max.regs.cnfgEmux.bit.calp_sel = 0; // Calibration disabled
	g_max.regs.cnfgEmux.bit.caln_sel = 0; // Calibration disabled

	_writeReg(MAX3001_REG_CNFG_EMUX, g_max.regs.cnfgEmux.all);

	g_max.regs.cnfgGen.all = _readReg(MAX3001_REG_CNFG_GEN);

	g_max.regs.cnfgGen.bit.en_ecg = 1; // Enable ECG

	_writeReg(MAX3001_REG_CNFG_GEN, g_max.regs.cnfgGen.all);

	// Wait until PLL is initialized
	max30001_timeout = 0;
	do {
		g_max.regs.status.all = _readReg(MAX3001_REG_STATUS);
	} while (g_max.regs.status.bit.pllint == 1 && max30001_timeout++ <= 1000);

	g_max.regs.mngrInt.all = _readReg(MAX3001_REG_MNGR_DYN);

	g_max.regs.mngrInt.bit.efit = 0x0F; // Set ECG FIFO to xxx

	_writeReg(MAX3001_REG_MNGR_INT, g_max.regs.mngrInt.all);

	g_max.regs.cnfgEcg.all = _readReg(MAX3001_REG_CNFG_ECG);

	g_max.regs.cnfgEcg.bit.dlpf = 0x01;
	g_max.regs.cnfgEcg.bit.dhpf = 0x01;
	g_max.regs.cnfgEcg.bit.rate = 0x02; // 128 SPS
	g_max.regs.cnfgEcg.bit.gain = 0x00; // 20V/V

	_writeReg(MAX3001_REG_CNFG_ECG, g_max.regs.cnfgEcg.all);

	_writeReg(MAX3001_REG_FIFO_RST, 0x00000000);
	_writeReg(MAX3001_REG_SYNCH, 0x00000000);
}

void max30001_stop_ecg(void)
{

}

void max30001_stop_bioz(void)
{

}

void max30001_start_bioz(void)
{

}

void max30001_start_rtor(void)
{

}

void max30001_stop_rtor(void)
{

}



void max30001_set_ecg_bioz_r2r_cfg(void)
{
	// clear all register values
	g_max.regs.enInt.all       = 0;
	g_max.regs.enInt2.all      = 0;
	g_max.regs.mngrInt.all     = 0;
	g_max.regs.mngrDyn.all     = 0;
	g_max.regs.cnfgGen.all     = 0;
	g_max.regs.cnfgCal.all     = 0;
	g_max.regs.cnfgEmux.all    = 0;
	g_max.regs.cnfgEcg.all     = 0;
	g_max.regs.cnfgBmux.all    = 0;
	g_max.regs.cnfgBioz.all    = 0;
	g_max.regs.cnfgBiozLc.all  = 0;
	g_max.regs.cnfgRtor1.all   = 0;
	g_max.regs.cnfg_rtor2.all  = 0;

	// set specific bits
	g_max.regs.enInt.bit.intb_type  = 0x03;
	g_max.regs.enInt2.bit.intb_type = 0x03;

	g_max.regs.mngrInt.bit.clr_samp = 0x01;
	g_max.regs.mngrInt.bit.bfit    = 0x07;
	g_max.regs.mngrInt.bit.efit    = 0x0F;

	g_max.regs.mngrDyn.bit.bloff_lo_it  = 0xFF;
	g_max.regs.mngrDyn.bit.bloff_hi_it  = 0xFF;
	g_max.regs.mngrDyn.bit.fast_th      = 0x3F;

	g_max.regs.cnfgGen.bit.rbiasn   = 0x01;
	g_max.regs.cnfgGen.bit.rbiasp   = 0x01;
	g_max.regs.cnfgGen.bit.rbiasv   = 0x01;
	g_max.regs.cnfgGen.bit.en_rbias = 0x02;
	g_max.regs.cnfgGen.bit.en_bloff = 0x03;
	g_max.regs.cnfgGen.bit.en_bioz  = 0x01;
	g_max.regs.cnfgGen.bit.en_ecg   = 0x01;

	g_max.regs.cnfgCal.bit.en_vcal= 0x01;
	g_max.regs.cnfgCal.bit.vmode  = 0x01;
	g_max.regs.cnfgCal.bit.fifty  = 0x01;
	g_max.regs.cnfgCal.bit.fcal   = 0x04;

	g_max.regs.cnfgEcg.bit.dlpf = 0x01;
	g_max.regs.cnfgEcg.bit.dhpf = 0x01;

	g_max.regs.cnfgBmux.bit.rmod    = 0x00;//0x04;
	g_max.regs.cnfgBmux.bit.cg_mode = 0x03;

	g_max.regs.cnfgBioz.bit.cgmag = 0x01;
	g_max.regs.cnfgBioz.bit.fcgen = 4;//0x0A;
	g_max.regs.cnfgBioz.bit.gain  = 0;//0x01;
	g_max.regs.cnfgBioz.bit.ahpf  = 0x06;

	g_max.regs.cnfgBiozLc.bit.cmag = 0x02;
	g_max.regs.cnfgBiozLc.bit.cmres = 0x08;
	g_max.regs.cnfgBiozLc.bit.hilob = 0x01;

	g_max.regs.cnfgRtor1.bit.ptsf = 0x03;
	g_max.regs.cnfgRtor1.bit.pavg = 0x02;
	g_max.regs.cnfgRtor1.bit.en_rtor = 0x01;
	g_max.regs.cnfgRtor1.bit.gain = 0x0F;
	g_max.regs.cnfgRtor1.bit.wndw = 0x03;

	g_max.regs.cnfg_rtor2.bit.rhsf = 0x04;
	g_max.regs.cnfg_rtor2.bit.ravg = 0x02;
	g_max.regs.cnfg_rtor2.bit.hoff = 0x20;

	INFO("write MAX3001_REG_EN_INT reg:       %08" PRIx32"\n", g_max.regs.enInt.all);
	INFO("write MAX3001_REG_EN_INT2 reg:      %08" PRIx32"\n", g_max.regs.enInt2.all);
	INFO("write MAX3001_REG_MNGR_INT reg:     %08" PRIx32"\n", g_max.regs.mngrInt.all);
	INFO("write MAX3001_REG_MNGR_DYN reg:     %08" PRIx32"\n", g_max.regs.mngrDyn.all);
	INFO("write MAX3001_REG_CNFG_GEN reg:     %08" PRIx32"\n", g_max.regs.cnfgGen.all);
	INFO("write MAX3001_REG_CNFG_CAL reg:     %08" PRIx32"\n", g_max.regs.cnfgCal.all);
	INFO("write MAX3001_REG_CNFG_EMUX reg:    %08" PRIx32"\n", g_max.regs.cnfgEmux.all);
	INFO("write MAX3001_REG_CNFG_ECG reg:     %08" PRIx32"\n", g_max.regs.cnfgEcg.all);
	INFO("write MAX3001_REG_CNFG_BMUX reg:    %08" PRIx32"\n", g_max.regs.cnfgBmux.all);
	INFO("write MAX3001_REG_CNFG_BIOZ reg:    %08" PRIx32"\n", g_max.regs.cnfgBioz.all);
	INFO("write MAX3001_REG_CNFG_BIOZ_LC reg: %08" PRIx32"\n", g_max.regs.cnfgBiozLc.all);
	INFO("write MAX3001_REG_CNFG_RTOR1 reg:   %08" PRIx32"\n", g_max.regs.cnfgRtor1.all);
	INFO("write MAX3001_REG_CNFG_RTOR2 reg:   %08" PRIx32"\n", g_max.regs.cnfg_rtor2.all);

	_writeReg(MAX3001_REG_EN_INT, g_max.regs.enInt.all);
	_writeReg(MAX3001_REG_EN_INT2, g_max.regs.enInt2.all);
	_writeReg(MAX3001_REG_MNGR_INT, g_max.regs.mngrInt.all);
	_writeReg(MAX3001_REG_MNGR_DYN, g_max.regs.mngrDyn.all);
	_writeReg(MAX3001_REG_CNFG_GEN, g_max.regs.cnfgGen.all);
	_writeReg(MAX3001_REG_CNFG_CAL, g_max.regs.cnfgCal.all);
	_writeReg(MAX3001_REG_CNFG_EMUX, g_max.regs.cnfgEmux.all);
	_writeReg(MAX3001_REG_CNFG_ECG, g_max.regs.cnfgEcg.all);
	_writeReg(MAX3001_REG_CNFG_BMUX, g_max.regs.cnfgBmux.all);
	_writeReg(MAX3001_REG_CNFG_BIOZ, g_max.regs.cnfgBioz.all);
	_writeReg(MAX3001_REG_CNFG_BIOZ_LC, g_max.regs.cnfgBiozLc.all);
	_writeReg(MAX3001_REG_CNFG_RTOR1, g_max.regs.cnfgRtor1.all);
	_writeReg(MAX3001_REG_CNFG_RTOR2, g_max.regs.cnfg_rtor2.all);
}

void max30001_get_ecg(void)
{

}

void max300001_get_status(void)
{
	int32_t ecg_val;
	float ecg_mv;
	g_max.regs.status.all = _readReg(0x01);
	// INFO("MAX3001_REG_STATUS reg read:     %08" PRIx32"\n",g_max.regs.status.all);
	if (g_max.regs.status.bit.eint == 1) {
		g_max.regs.ecg_fifo.all = _readReg(MAX3001_REG_ECG_FIFO);
		ecg_val = g_max.regs.ecg_fifo.all & 0xFFFFFF30;
		ecg_val = ecg_val << 8;
		ecg_val = ecg_val / 16384;
		ecg_val = ecg_val * 1000;
		ecg_mv = (float)ecg_val / (float)2621440;

		//ecg_mv = ((float)(g_max.regs.ecf_fifo.all>>6)*1000)/(131072*20);
		// ecg_mv = (float)(g_max.regs.ecg_fifo.all>>6);
		// ecg_mv *= 1000;
		// ecg_mv /= (131072*20);
		INFO(" %.8f\n", ecg_mv);
		//INFO("%d,%d,%d\n",(int)g_max.regs.ecg_fifo.all>>6,(int)g_max.regs.ecg_fifo.bit.etag,(int)g_max.regs.ecg_fifo.bit.ptag);
		//INFO("MAX30001_FIFO reg read:         %08" PRIx32"\n",g_max.regs.ecf_fifo.all);
		//INFO("MAX30001_ECG data, etag, ptag %d,%04 "PRIx32 "%04" PRIx32"\n",g_max.regs.ecf_fifo.bit.ecg_data,g_max.regs.ecf_fifo.bit.etag,g_max.regs.ecf_fifo.bit.ptag);
		//INFO("%08"PRIx32",%08"PRIx32"\n",g_max.regs.status.all,g_max.regs.ecg_fifo.all);
		//INFO("%d\n",(int)ecg_val);
	}
	//max30001_get_ecg();
}

static void _init(void)
{
	uint32_t regVal;
	esp_err_t ret;
	// spi_device_handle_t spi;
	spi_bus_config_t buscfg = {
		.miso_io_num = SPI_PIN_NUM_MISO,
		.mosi_io_num = SPI_PIN_NUM_MOSI,
		.sclk_io_num = SPI_PIN_NUM_CLK,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.max_transfer_sz = 0
	};
	spi_device_interface_config_t devcfg = {
#ifdef CONFIG_LCD_OVERCLOCK
		.clock_speed_hz = 26 * 1000 * 1000,     //Clock out at 26 MHz
#else
		.clock_speed_hz = 1 * 1000 * 1000,     //Clock out at 10 MHz
#endif
		.mode = 0,                              //SPI mode 0
		.spics_io_num = SPI_PIN_NUM_CS,             //CS pin
		.queue_size = 7,                        //We want to be able to queue 7 transactions at a time
		//.pre_cb=lcd_spi_pre_transfer_callback,  //Specify pre-transfer callback to handle D/C line
	};
	//Initialize the SPI bus
	ret = spi_bus_initialize(SPI_MAX30001_HOST, &buscfg, SPI_DMA_CH_AUTO);
	ESP_ERROR_CHECK(ret);
	//Attach the MAX30001 to the SPI bus
	ret = spi_bus_add_device(SPI_MAX30001_HOST, &devcfg, &g_max.spi_handle);
	ESP_ERROR_CHECK(ret);

	g_max.semaphore = xSemaphoreCreateBinary();
	xSemaphoreGive(g_max.semaphore);

	g_max.regs.status.all = _readReg(0x01);
	INFO("MAX3001_REG_STATUS reg read:     %08" PRIx32"\n", g_max.regs.status.all);

	ret = xTaskCreate(_task, "max30001", 8192, NULL, 3, NULL);
	if (ret != pdPASS) {
		ERROR("create task %s failed\n", "max30001");
		return;
	}
}

static bool dbgRd(uint8_t argc, char** argv)
{
	uint8_t   reg;
	uint32_t  val;

	if (argc < 2) {
		return false;
	}

	reg = strtoul(argv[1], NULL, 16);
	val = _readReg(reg);
	PRINT("%x\n", val);

	switch (reg) {
		case MAX3001_REG_STATUS:
			max30001_status_u* status = (max30001_status_u*)&val;
			PRINT("EINT     : %d\n", status->bit.eint);
			PRINT("EOVF     : %d\n", status->bit.eovf);
			PRINT("FSTINT   : %d\n", status->bit.fstint);
			PRINT("DCLOFFINT: %d\n", status->bit.dcloffint);
			PRINT("BINT     : %d\n", status->bit.bint);
			PRINT("BOVF     : %d\n", status->bit.bovf);
			PRINT("BOVER    : %d\n", status->bit.bover);
			PRINT("BUNDR    : %d\n", status->bit.bundr);
			PRINT("BCGMON   : %d\n", status->bit.bcgmon);
			PRINT("PINT     : %d\n", status->bit.pint);
			PRINT("POVF     : %d\n", status->bit.povf);
			PRINT("PEDGE    : %d\n", status->bit.pedge);
			PRINT("LONINT   : %d\n", status->bit.lonint);
			PRINT("RRINT    : %d\n", status->bit.rrint);
			PRINT("SAMP     : %d\n", status->bit.samp);
			PRINT("PLLINT   : %d\n", status->bit.pllint);
			PRINT("BCGMN    : %d\n", status->bit.bcgmn);
			PRINT("BCGMP    : %d\n", status->bit.bcgmp);
			PRINT("BCGMN    : %d\n", status->bit.bcgmn);
			PRINT("LDOFF_PH : %d\n", status->bit.ldoff_ph);
			PRINT("LDOFF_PL : %d\n", status->bit.ldoff_pl);
			PRINT("LDOFF_NH : %d\n", status->bit.ldoff_nh);
			PRINT("LDOFF_NL : %d\n", status->bit.ldoff_nl);
			break;

		case MAX3001_REG_MNGR_INT:
			max30001_mngr_int_u* mngr = (max30001_mngr_int_u*)&val;
			PRINT("EFIT      : %x\n", mngr->bit.efit);
			PRINT("BFIT      : %x\n", mngr->bit.bfit);
			PRINT("CLR_FAST  : %x\n", mngr->bit.clr_fast);
			PRINT("CLR_RRINT : %x\n", mngr->bit.clr_rrint);
			PRINT("CLR_PEDGE : %x\n", mngr->bit.clr_pedge);
			PRINT("CLR_SAMP  : %x\n", mngr->bit.clr_samp);
			PRINT("SAMP_IT   : %x\n", mngr->bit.samp_it);
			break;

		case MAX3001_REG_CNFG_GEN:
			max30001_cnfg_gen_u* cnfg = (max30001_cnfg_gen_u*)&val;
			PRINT("CNFG_GEN    : %x\n", cnfg->bit.en_ulp_lon);
			PRINT("FMSTR       : %x\n", cnfg->bit.fmstr);
			PRINT("EN_ECG      : %x\n", cnfg->bit.en_ecg);
			PRINT("EN_BIOZ     : %x\n", cnfg->bit.en_bioz);
			PRINT("EN_PACE     : %x\n", cnfg->bit.en_pace);
			PRINT("EN_BLOFF    : %x\n", cnfg->bit.en_bloff);
			PRINT("EN_DCLOFF   : %x\n", cnfg->bit.en_dcloff);
			PRINT("DCLOFF_IPOL : %x\n", cnfg->bit.dcloff_ipol);
			PRINT("IMAG        : %x\n", cnfg->bit.imag);
			PRINT("VTH         : %x\n", cnfg->bit.vth);
			PRINT("EN_RBIAS    : %x\n", cnfg->bit.en_rbias);
			PRINT("RBIASV      : %x\n", cnfg->bit.rbiasv);
			PRINT("RBIASP      : %x\n", cnfg->bit.rbiasp);
			PRINT("RBIASN      : %x\n", cnfg->bit.rbiasn);
			break;

		case MAX3001_REG_CNFG_BIOZ:
			max30001_cnfg_bioz_u* boiz = (max30001_cnfg_bioz_u*)&val;
			PRINT("BIOZ_RATE    : %x\n", boiz->bit.rate);
			PRINT("BIOZ_AHPF    : %x\n", boiz->bit.ahpf);
			PRINT("EXT_RBIAS    : %x\n", boiz->bit.ext_rbias);
			PRINT("LN_BIOZ      : %x\n", boiz->bit.ln_bioz);
			PRINT("BIOZ_GAIN    : %x\n", boiz->bit.gain);
			PRINT("BIOZ_DHPF    : %x\n", boiz->bit.dhpf);
			PRINT("BIOZ_DLPF    : %x\n", boiz->bit.dlpf);
			PRINT("BIOZ_FCGEN   : %x\n", boiz->bit.fcgen);
			PRINT("BIOZ_CGMON   : %x\n", boiz->bit.cgmon);
			PRINT("BIOZ_CGMAG   : %x\n", boiz->bit.cgmag);
			PRINT("BIOZ_PHOFF   : %x\n", boiz->bit.phoff);
			break;
	}

	return true;
}

static bool dbgWr(uint8_t argc, char** argv)
{
	uint8_t   reg;
	uint32_t  val;

	if (argc < 3) {
		return false;
	}

	reg = strtoul(argv[1], NULL, 16);
	val = strtoul(argv[2], NULL, 16);

	_writeReg(reg, val);
	return true;
}

static bool dbgWrBits(uint8_t argc, char** argv)
{
	uint8_t		reg;
	uint32_t	val;
	uint32_t	bitval;
	uint32_t	mask;
	uint8_t		bit;
	uint8_t		size;

	if (argc < 5) {
		return false;
	}

	reg    = strtoul(argv[1], NULL, 16);
	bit    = strtoul(argv[2], NULL, 10);
	size   = strtoul(argv[3], NULL, 10);
	bitval = strtoul(argv[4], NULL, 16);

	val = _readReg(reg);

	mask = ~(((1<<size)-1) << bit);
	PRINT("current: %x, mas:%x\n", val, mask);

	val &= mask;
	val |= (bitval << bit);
	PRINT("setting to %x\n", val);

	_writeReg(reg, val);
	return true;
}

static bool dbgRdBits(uint8_t argc, char** argv)
{
	uint8_t		reg;
	uint32_t	val;
	uint32_t	bitval;
	uint32_t	mask;
	uint8_t		bit;
	uint8_t		size;

	if (argc < 4) {
		return false;
	}

	reg    = strtoul(argv[1], NULL, 16);
	bit    = strtoul(argv[2], NULL, 10);
	size   = strtoul(argv[3], NULL, 10);

	val = _readReg(reg);

	bitval = BITFIELD_GET(val, bit, size);

	PRINT("%x %x\n", val, bitval);

	return true;
}



static bool dbgEcgStart(uint8_t argc, char** argv)
{
	max30001_start_ecg();
	return true;
}

static bool dbgReset(uint8_t argc, char** argv)
{
	_writeReg(MAX3001_REG_SW_RST, 0);
	return true;
}

static bool dbgSync(uint8_t argc, char** argv)
{
	_writeReg(MAX3001_REG_SYNCH, 0);
	return true;
}

static bool dbgStartBioz(uint8_t argc, char** argv)
{
	max30001_set_ecg_bioz_r2r_cfg();

	return true;
}


static bool dbgStatus(uint8_t argc, char** argv)
{
	uint32_t	val;

	val = _readReg(MAX3001_REG_INFO);
	PRINT("%x\n", val);

	PRINT("ecg rcv count: %d\n", g_max.ecg.rcvCount);
	g_max.ecg.rcvCount = 0;

	return true;
}

// *INDENT-OFF*
DEBUG_MENU_START(g_menu)
	DEBUG_MENU_DIR("max30001",	NULL)
		DEBUG_MENU_CMD("status",	NULL,		NULL, dbgStatus)
		DEBUG_MENU_CMD("r",			NULL,		NULL, dbgRd)
		DEBUG_MENU_CMD("w",			NULL,		NULL, dbgWr)
		DEBUG_MENU_CMD("rbits",		NULL,		NULL, dbgRdBits)
		DEBUG_MENU_CMD("wbits",		NULL,		NULL, dbgWrBits)
		DEBUG_MENU_CMD("ecgStart",	NULL,		NULL, dbgEcgStart)
		DEBUG_MENU_CMD("reset",		NULL,		NULL, dbgReset)
		DEBUG_MENU_CMD("sync",		NULL,		NULL, dbgSync)
		DEBUG_MENU_CMD("bioz",		NULL,		NULL, dbgStartBioz)
	DEBUG_MENU_DIR_END
DEBUG_MENU_END
// *INDENT-ON*


void max30001_init(void)
{
	DBG_TREE_add("/", g_menu);

	_init();

	max30001_start_ecg();
	max30001_set_ecg_bioz_r2r_cfg();
}
