
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

#define MAX30001_HOST    SPI2_HOST

#define CES_CMDIF_PKT_START_1 0x0A
#define CES_CMDIF_PKT_START_2 0xFA
#define CES_CMDIF_TYPE_DATA 0x02
#define CES_CMDIF_PKT_STOP 0x0B
#define DATA_LEN 0x0C
#define ZERO 0



  typedef enum { // MAX30001 Register addresses
    STATUS     = 0x01,
    EN_INT     = 0x02,
    EN_INT2    = 0x03,
    MNGR_INT   = 0x04,
    MNGR_DYN   = 0x05,
    SW_RST     = 0x08,
    SYNCH      = 0x09,
    FIFO_RST   = 0x0A,
    INFO       = 0x0F,
    CNFG_GEN   = 0x10,
    CNFG_CAL   = 0x12,
    CNFG_EMUX  = 0x14,
    CNFG_ECG   = 0x15,
    CNFG_BMUX  = 0x17,
    CNFG_BIOZ  = 0x18,
    CNFG_PACE  = 0x1A,
    CNFG_RTOR1 = 0x1D,
    CNFG_RTOR2 = 0x1E,

    // Data locations
    ECG_FIFO_BURST = 0x20,
    ECG_FIFO       = 0x21,
    FIFO_BURST     = 0x22,
    BIOZ_FIFO      = 0x23,
    RTOR           = 0x25,

    PACE0_FIFO_BURST = 0x30,
    PACE0_A          = 0x31,
    PACE0_B          = 0x32,
    PACE0_C          = 0x33,

    PACE1_FIFO_BURST = 0x34,
    PACE1_A          = 0x35,
    PACE1_B          = 0x36,
    PACE1_C          = 0x37,

    PACE2_FIFO_BURST = 0x38,
    PACE2_A          = 0x39,
    PACE2_B          = 0x3A,
    PACE2_C          = 0x3B,

    PACE3_FIFO_BURST = 0x3C,
    PACE3_A          = 0x3D,
    PACE3_B          = 0x3E,
    PACE3_C          = 0x3F,

    PACE4_FIFO_BURST = 0x40,
    PACE4_A          = 0x41,
    PACE4_B          = 0x42,
    PACE4_C          = 0x43,

    PACE5_FIFO_BURST = 0x44,
    PACE5_A          = 0x45,
    PACE5_B          = 0x46,
    PACE5_C          = 0x47,

  } MAX30001_REG_map_t;

  /**
   * @brief STATUS (0x01) 
   */
  typedef union max30001_status_reg {
    uint32_t all;

    struct {
      uint32_t loff_nl : 1;
      uint32_t loff_nh : 1;
      uint32_t loff_pl : 1;
      uint32_t loff_ph : 1;

      uint32_t bcgmn     : 1;
      uint32_t bcgmp     : 1;
      uint32_t reserved1 : 1;
      uint32_t reserved2 : 1;

      uint32_t pllint : 1;
      uint32_t samp   : 1;
      uint32_t rrint  : 1;
      uint32_t lonint : 1;

      uint32_t pedge  : 1;
      uint32_t povf   : 1;
      uint32_t pint   : 1;
      uint32_t bcgmon : 1;

      uint32_t bundr : 1;
      uint32_t bover : 1;
      uint32_t bovf  : 1;
      uint32_t bint  : 1;

      uint32_t dcloffint : 1;
      uint32_t fstint    : 1;
      uint32_t eovf      : 1;
      uint32_t eint      : 1;

      uint32_t reserved : 8;

    } bit;

  } max30001_status_u;

  
  /**
   * @brief EN_INT (0x02) 
   */

  typedef union max30001_en_int_reg {
    uint32_t all;

    struct {
      uint32_t intb_type : 2;
      uint32_t reserved1 : 1;
      uint32_t reserved2 : 1;

      uint32_t reserved3 : 1;
      uint32_t reserved4 : 1;
      uint32_t reserved5 : 1;
      uint32_t reserved6 : 1;

      uint32_t en_pllint : 1;
      uint32_t en_samp   : 1;
      uint32_t en_rrint  : 1;
      uint32_t en_lonint : 1;

      uint32_t en_pedge  : 1;
      uint32_t en_povf   : 1;
      uint32_t en_pint   : 1;
      uint32_t en_bcgmon : 1;

      uint32_t en_bundr : 1;
      uint32_t en_bover : 1;
      uint32_t en_bovf  : 1;
      uint32_t en_bint  : 1;

      uint32_t en_dcloffint : 1;
      uint32_t en_fstint    : 1;
      uint32_t en_eovf      : 1;
      uint32_t en_eint      : 1;

      uint32_t reserved : 8;

    } bit;

  } max30001_en_int_u;

  
  /**
   * @brief EN_INT2 (0x03) 
   */  
  typedef union max30001_en_int2_reg {
    uint32_t all;

    struct {
      uint32_t intb_type : 2;
      uint32_t reserved1 : 1;
      uint32_t reserved2 : 1;

      uint32_t reserved3 : 1;
      uint32_t reserved4 : 1;
      uint32_t reserved5 : 1;
      uint32_t reserved6 : 1;

      uint32_t en_pllint : 1;
      uint32_t en_samp   : 1;
      uint32_t en_rrint  : 1;
      uint32_t en_lonint : 1;

      uint32_t en_pedge  : 1;
      uint32_t en_povf   : 1;
      uint32_t en_pint   : 1;
      uint32_t en_bcgmon : 1;

      uint32_t en_bundr  : 1;
      uint32_t en_bover  : 1;
      uint32_t en_bovf   : 1;
      uint32_t en_bint   : 1;

      uint32_t en_dcloffint : 1;
      uint32_t en_fstint    : 1;
      uint32_t en_eovf      : 1;
      uint32_t en_eint      : 1;

      uint32_t reserved : 8;

    } bit;

  } max30001_en_int2_u;

  /**
   * @brief MNGR_INT (0x04) 
   */  
  typedef union max30001_mngr_int_reg {
    uint32_t all;

    struct {
      uint32_t samp_it   : 2;
      uint32_t clr_samp  : 1;
      uint32_t reserved0 : 1;
      uint32_t clr_rrint : 2;
      uint32_t clr_fast  : 1;
      uint32_t reserved1 : 1;
      uint32_t reserved2 : 4;
      uint32_t reserved3 : 4;
      uint32_t b_fit     : 3;
      uint32_t e_fit     : 5;

      uint32_t reserved : 8;

    } bit;

  } max30001_mngr_int_u;

   /**
   * @brief MNGR_DYN (0x05) 
   */ 
  typedef union max30001_mngr_dyn_reg {
    uint32_t all;

    struct {
      uint32_t bloff_lo_it : 8;
      uint32_t bloff_hi_it : 8;
      uint32_t fast_th     : 6;
      uint32_t fast        : 2;
      uint32_t reserved    : 8;
    } bit;

  } max30001_mngr_dyn_u;

  // 0x08
  // uint32_t max30001_sw_rst;

  // 0x09
  // uint32_t max30001_synch;

  // 0x0A
  // uint32_t max30001_fifo_rst;

  
   /**
   * @brief INFO (0x0F) 
   */
  typedef union max30001_info_reg {
    uint32_t all;
    struct {
      uint32_t serial    : 12;
      uint32_t part_id   : 2;
      uint32_t sample    : 1;
      uint32_t reserved1 : 1;
      uint32_t rev_id    : 4;
      uint32_t pattern   : 4;
      uint32_t reserved  : 8;
    } bit;

  } max30001_info_u;

   /**
   * @brief CNFG_GEN (0x10) 
   */
  typedef union max30001_cnfg_gen_reg {
    uint32_t all;
    struct {
      uint32_t rbiasn     : 1;
      uint32_t rbiasp     : 1;
      uint32_t rbiasv     : 2;
      uint32_t en_rbias   : 2;
      uint32_t vth        : 2;
      uint32_t imag       : 3;
      uint32_t ipol       : 1;
      uint32_t en_dcloff  : 2;
      uint32_t en_bloff   : 2;
      uint32_t reserved1  : 2;
      uint32_t en_bioz    : 1;
      uint32_t en_ecg     : 1;
      uint32_t fmstr      : 2;
      uint32_t en_ulp_lon : 2;
      uint32_t reserved : 8;
    } bit;

  } max30001_cnfg_gen_u;

  
   /**
   * @brief CNFG_CAL (0x12) 
   */  
  typedef union max30001_cnfg_cal_reg {
    uint32_t all;
    struct {
      uint32_t thigh     : 11;
      uint32_t fifty     : 1;
      uint32_t fcal      : 3;
      uint32_t reserved1 : 5;
      uint32_t vmag      : 1;
      uint32_t vmode     : 1;
      uint32_t en_vcal   : 1;
      uint32_t reserved2 : 1;
      uint32_t reserved  : 8;
    } bit;

  } max30001_cnfg_cal_u;

   /**
   * @brief CNFG_EMUX  (0x14) 
   */
  typedef union max30001_cnfg_emux_reg {
    uint32_t all;
    struct {
      uint32_t reserved1 : 16;
      uint32_t caln_sel  : 2;
      uint32_t calp_sel  : 2;
      uint32_t openn     : 1;
      uint32_t openp     : 1;
      uint32_t reserved2 : 1;
      uint32_t pol       : 1;
      uint32_t reserved : 8;
    } bit;

  } max30001_cnfg_emux_u;

  
   /**
   * @brief CNFG_ECG   (0x15) 
   */  
  typedef union max30001_cnfg_ecg_reg {
    uint32_t all;
    struct {
      uint32_t reserved1 : 12;
      uint32_t dlpf      : 2;
      uint32_t dhpf      : 1;
      uint32_t reserved2 : 1;
      uint32_t gain      : 2;
      uint32_t reserved3 : 4;
      uint32_t rate      : 2;

      uint32_t reserved  : 8;
    } bit;

  } max30001_cnfg_ecg_u;

   /**
   * @brief CNFG_BMUX   (0x17) 
   */  
  typedef union max30001_cnfg_bmux_reg {
    uint32_t all;
    struct {
      uint32_t fbist     : 2;
      uint32_t reserved1 : 2;
      uint32_t rmod      : 3;
      uint32_t reserved2 : 1;
      uint32_t rnom      : 3;
      uint32_t en_bist   : 1;
      uint32_t cg_mode   : 2;
      uint32_t reserved3 : 2;
      uint32_t caln_sel  : 2;
      uint32_t calp_sel  : 2;
      uint32_t openn     : 1;
      uint32_t openp     : 1;
      uint32_t reserved4 : 2;
      uint32_t reserved : 8;
    } bit;

  } max30001_cnfg_bmux_u;

   /**
   * @brief CNFG_BIOZ   (0x18) 
   */ 
  typedef union max30001_bioz_reg {
    uint32_t all;
    struct {
      uint32_t phoff     : 4;
      uint32_t cgmag     : 3;
      uint32_t cgmon     : 1;
      uint32_t fcgen     : 4;
      uint32_t dlpf      : 2;
      uint32_t dhpf      : 2;
      uint32_t gain      : 2;
      uint32_t inapow_mode : 1;
      uint32_t ext_rbias : 1;
      uint32_t ahpf      : 3;
      uint32_t rate      : 1;
      uint32_t reserved : 8;
    } bit;

  } max30001_cnfg_bioz_u;

  
   /**
   * @brief CNFG_BIOZ_LC  (0x1A) 
   */   
  typedef union max30001_cnfg_bioz_lc_reg {
    uint32_t all;

    struct {
      uint32_t cmag        : 4;
      uint32_t cmres       : 4;
      uint32_t reserved1   : 4;
      uint32_t bistr       : 2;
      uint32_t enbistr     : 1;
      uint32_t reserved2   : 4;
      uint32_t lc2x        : 1;
      uint32_t reserved3   : 3;
      uint32_t hilob       : 1;
      uint32_t reserved    : 8;
    } bit;

  } max30001_cnfg_bioz_lc_u;

   /**
   * @brief CNFG_RTOR1   (0x1D) 
   */   
  typedef union max30001_cnfg_rtor1_reg {
    uint32_t all;
    struct {
      uint32_t reserved1 : 8;
      uint32_t ptsf      : 4;
      uint32_t pavg      : 2;
      uint32_t reserved2 : 1;
      uint32_t en_rtor   : 1;
      uint32_t gain      : 4;
      uint32_t wndw      : 4;
      uint32_t reserved : 8;
    } bit;

  } max30001_cnfg_rtor1_u;

   /**
   * @brief CNFG_RTOR2 (0x1E) 
   */   
  typedef union max30001_cnfg_rtor2_reg {
    uint32_t all;
    struct {
      uint32_t reserved1 : 8;
      uint32_t rhsf      : 3;
      uint32_t reserved2 : 1;
      uint32_t ravg      : 2;
      uint32_t reserved3 : 2;
      uint32_t hoff      : 6;
      uint32_t reserved4 : 2;
      uint32_t reserved : 8;
    } bit;

  } max30001_cnfg_rtor2_u;

     /**
   * @brief ECG_FIFO (0x21) 
   */   
  typedef union max30001_ecg_fifo_reg {
    uint32_t all;
    struct {
      uint32_t ecg_data  : 18;
      uint32_t etag      : 3;
      uint32_t ptag      : 3;
      uint32_t reserved : 8;
    } bit;

  } max30001_ecg_fifo_u;




const char DataPacketFooter[2] = {ZERO, CES_CMDIF_PKT_STOP};
const char DataPacketHeader[5] = {CES_CMDIF_PKT_START_1, CES_CMDIF_PKT_START_2, DATA_LEN, ZERO, CES_CMDIF_TYPE_DATA};

uint8_t data_len = 0x0C;

#define MAX30001_WREG   0x00
#define MAX30001_RREG  0x01

#define MAX30001_STATUS_REG             0x01
#define MAX30001_EN_INT_REG             0x02
#define MAX30001_EN_INT2_REG            0x03
#define MAX30001_MNGR_INT_REG           0x04
#define MAX30001_MNGR_DYN_REG           0x05
#define MAX30001_SW_RST_REG             0x08
#define MAX30001_SYNCH_REG              0x09
#define MAX30001_FIFO_RST_REG           0x0A
#define MAX30001_INFO_REG               0x0F
#define MAX30001_CNFG_GEN_REG           0x10
#define MAX30001_CNFG_CAL_REG           0x12
#define MAX30001_CNFG_EMUX_REG          0x14
#define MAX30001_CNFG_ECG_REG           0x15
#define MAX30001_CNFG_BMUX_REG          0x17
#define MAX30001_CNFG_BIOZ_REG          0x18
#define MAX30001_CNFG_BIOZ_LC_REG       0x1A
#define MAX30001_CNFG_RTOR1_REG         0x1D
#define MAX30001_CNFG_RTOR2_REG         0x1E
#define MAX30001_ECG_FIFO_BURST_REG     0x20
#define MAX30001_ECG_FIFO_REG           0x21
#define MAX30001_BIOZ_FIFO_BURST_REG    0x22
#define MAX30001_BIOZ_FIFO_REG          0x23
#define MAX30001_RTOR_REG               0x25
#define MAX30001_NO_OP_REG              0x7F

typedef struct max30001_registers
{
  max30001_en_int_u enInt;
  max30001_en_int2_u enInt2;
  max30001_mngr_int_u mngrInt;
  max30001_mngr_dyn_u mngrDyn;
  max30001_info_u info;
  max30001_cnfg_gen_u cnfgGen;
  max30001_cnfg_cal_u cnfgCal;
  max30001_cnfg_emux_u cnfgEmux;
  max30001_cnfg_ecg_u cnfgEcg;
  max30001_cnfg_bmux_u cnfgBmux;
  max30001_cnfg_bioz_u cnfgBioz;
  max30001_cnfg_bioz_lc_u cnfgBiozLc;
  max30001_cnfg_rtor1_u cnfgRtor1;
  max30001_cnfg_rtor2_u cnfg_rtor2;
  max30001_status_u status;
  max30001_ecg_fifo_u ecg_fifo;
}max30001_registers_t;

 typedef enum {
    MAX30001_NO_INT = 0, // No interrupt
    MAX30001_INT_B  = 1,  // INTB selected for interrupt
    MAX30001_INT_2B = 2  // INT2B selected for interrupt
  } max30001_intrpt_Location_t;

static max30001_registers_t max_regs_val;

static spi_device_handle_t spi_handle;



static uint32_t _readReg(uint8_t i_reg)
{
  uint8_t cmd[4] = {((i_reg << 1) | MAX30001_RREG), 0x00,0x00,0x00}; 
  uint32_t retVal;
    // When using SPI_TRANS_CS_KEEP_ACTIVE, bus must be locked/acquired
    spi_device_acquire_bus(spi_handle, portMAX_DELAY);

    //max30001_cmd(i_reg << 1 | MAX30001_RREG, true);

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length=8*4;
    t.tx_buffer = cmd;
    t.flags = SPI_TRANS_USE_RXDATA;
    t.user = (void*)1;

    esp_err_t ret = spi_device_polling_transmit(spi_handle, &t);
    assert( ret == ESP_OK );

    // Release bus
    spi_device_release_bus(spi_handle);
    retVal = t.rx_data[0] << 24 | t.rx_data[1] << 16 | t.rx_data[2] << 8 | t.rx_data[3];
   

    return retVal;
}

static void _writeReg(uint8_t i_reg, uint32_t regVal)
{
  //uint8_t cmd[4] = {((i_reg << 1) | MAX30001_WREG), data[2],data[1],data[0]}; 

    //max30001_cmd(i_reg << 1 | MAX30001_WREG, true);
    spi_device_acquire_bus(spi_handle, portMAX_DELAY);

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length=8*4;
    t.flags = SPI_TRANS_USE_TXDATA;
    t.tx_data[0] = ((i_reg << 1) | MAX30001_WREG);
    t.tx_data[1] = regVal >> 16;
    t.tx_data[2] = regVal >> 8;
    t.tx_data[3] = regVal;
    t.user = (void*)1;

    esp_err_t ret = spi_device_polling_transmit(spi_handle, &t);
    assert( ret == ESP_OK );

    // Release bus
    spi_device_release_bus(spi_handle);

}

void max30001_start_ecg(void)
{
  uint32_t max30001_timeout = 0;

  max_regs_val.mngrInt.bit.clr_samp = 0x01;
  max_regs_val.mngrInt.bit.b_fit    = 0x07;
  max_regs_val.mngrInt.bit.e_fit    = 0x0F;

  _writeReg(MAX30001_MNGR_INT_REG,max_regs_val.mngrInt.all);

  max_regs_val.cnfgEmux.all = _readReg(MAX30001_CNFG_EMUX_REG);
  max_regs_val.cnfgEmux.bit.openp = 0; // Positive lead enabled
  max_regs_val.cnfgEmux.bit.openn = 0; // Negative lead enabled
  max_regs_val.cnfgEmux.bit.pol = 0;   // Positive polarity
  max_regs_val.cnfgEmux.bit.calp_sel = 0; // Calibration disabled
  max_regs_val.cnfgEmux.bit.caln_sel = 0; // Calibration disabled

  _writeReg(MAX30001_CNFG_EMUX_REG,max_regs_val.cnfgEmux.all);

  max_regs_val.cnfgGen.all = _readReg(MAX30001_CNFG_GEN_REG);

  max_regs_val.cnfgGen.bit.en_ecg = 1; // Enable ECG

  _writeReg(MAX30001_CNFG_GEN_REG,max_regs_val.cnfgGen.all);

  // Wait until PLL is initialized
  max30001_timeout = 0;
  do
  {
    max_regs_val.status.all = _readReg(MAX30001_STATUS_REG);
  } while (max_regs_val.status.bit.pllint == 1 && max30001_timeout++ <= 1000);

  max_regs_val.mngrInt.all = _readReg(MAX30001_MNGR_DYN_REG);

  max_regs_val.mngrInt.bit.e_fit = 0x0F; // Set ECG FIFO to xxx

  _writeReg(MAX30001_MNGR_INT_REG,max_regs_val.mngrInt.all);

  max_regs_val.cnfgEcg.all = _readReg(MAX30001_CNFG_ECG_REG);

  max_regs_val.cnfgEcg.bit.dlpf = 0x01;
  max_regs_val.cnfgEcg.bit.dhpf = 0x01;
  max_regs_val.cnfgEcg.bit.rate = 0x02; // 128 SPS
  max_regs_val.cnfgEcg.bit.gain = 0x00; // 20V/V

  _writeReg(MAX30001_CNFG_ECG_REG,max_regs_val.cnfgEcg.all);

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
  max_regs_val.enInt.all       = 0;
  max_regs_val.enInt2.all      = 0;
  max_regs_val.mngrInt.all     = 0;
  max_regs_val.mngrDyn.all     = 0;
  max_regs_val.cnfgGen.all     = 0;
  max_regs_val.cnfgCal.all     = 0;
  max_regs_val.cnfgEmux.all    = 0;
  max_regs_val.cnfgEcg.all     = 0;
  max_regs_val.cnfgBmux.all    = 0;
  max_regs_val.cnfgBioz.all    = 0;
  max_regs_val.cnfgBiozLc.all  = 0;
  max_regs_val.cnfgRtor1.all   = 0;
  max_regs_val.cnfg_rtor2.all  = 0;

// set specific bits
  max_regs_val.enInt.bit.intb_type  = 0x03;
  max_regs_val.enInt2.bit.intb_type = 0x03;

  max_regs_val.mngrInt.bit.clr_samp = 0x01;
  max_regs_val.mngrInt.bit.b_fit    = 0x07;
  max_regs_val.mngrInt.bit.e_fit    = 0x0F;

  max_regs_val.mngrDyn.bit.bloff_lo_it  = 0xFF;
  max_regs_val.mngrDyn.bit.bloff_hi_it  = 0xFF;
  max_regs_val.mngrDyn.bit.fast_th      = 0x3F;

  max_regs_val.cnfgGen.bit.rbiasn   = 0x01;
  max_regs_val.cnfgGen.bit.rbiasp   = 0x01;
  max_regs_val.cnfgGen.bit.rbiasv   = 0x01;
  max_regs_val.cnfgGen.bit.en_rbias = 0x02;
  max_regs_val.cnfgGen.bit.en_bioz  = 0x01;
  max_regs_val.cnfgGen.bit.en_ecg   = 0x01;

  max_regs_val.cnfgCal.bit.fifty  = 0x01;
  max_regs_val.cnfgCal.bit.fcal   = 0x04;

  max_regs_val.cnfgEcg.bit.dlpf = 0x01;
  max_regs_val.cnfgEcg.bit.dhpf = 0x01;

  max_regs_val.cnfgBmux.bit.rmod    = 0x04;
  max_regs_val.cnfgBmux.bit.cg_mode = 0x03;

  max_regs_val.cnfgBioz.bit.cgmag = 0x01;
  max_regs_val.cnfgBioz.bit.fcgen = 0x0A;
  max_regs_val.cnfgBioz.bit.gain  = 0x01;
  max_regs_val.cnfgBioz.bit.ahpf  = 0x06;

  max_regs_val.cnfgBiozLc.bit.cmag = 0x02;
  max_regs_val.cnfgBiozLc.bit.cmres = 0x08;
  max_regs_val.cnfgBiozLc.bit.hilob = 0x01;

  max_regs_val.cnfgRtor1.bit.ptsf = 0x03;
  max_regs_val.cnfgRtor1.bit.pavg = 0x02;
  max_regs_val.cnfgRtor1.bit.en_rtor = 0x01;
  max_regs_val.cnfgRtor1.bit.gain = 0x0F;
  max_regs_val.cnfgRtor1.bit.wndw = 0x03;

  max_regs_val.cnfg_rtor2.bit.rhsf = 0x04;
  max_regs_val.cnfg_rtor2.bit.ravg = 0x02;
  max_regs_val.cnfg_rtor2.bit.hoff = 0x20;

  INFO("write MAX30001_EN_INT_REG reg:       %08" PRIx32"\n",max_regs_val.enInt.all);
  INFO("write MAX30001_EN_INT2_REG reg:      %08" PRIx32"\n",max_regs_val.enInt2.all);
  INFO("write MAX30001_MNGR_INT_REG reg:     %08" PRIx32"\n",max_regs_val.mngrInt.all);
  INFO("write MAX30001_MNGR_DYN_REG reg:     %08" PRIx32"\n",max_regs_val.mngrDyn.all);
  INFO("write MAX30001_CNFG_GEN_REG reg:     %08" PRIx32"\n",max_regs_val.cnfgGen.all);
  INFO("write MAX30001_CNFG_CAL_REG reg:     %08" PRIx32"\n",max_regs_val.cnfgCal.all);
  INFO("write MAX30001_CNFG_EMUX_REG reg:    %08" PRIx32"\n",max_regs_val.cnfgEmux.all);
  INFO("write MAX30001_CNFG_ECG_REG reg:     %08" PRIx32"\n",max_regs_val.cnfgEcg.all);
  INFO("write MAX30001_CNFG_BMUX_REG reg:    %08" PRIx32"\n",max_regs_val.cnfgBmux.all);
  INFO("write MAX30001_CNFG_BIOZ_REG reg:    %08" PRIx32"\n",max_regs_val.cnfgBioz.all);
  INFO("write MAX30001_CNFG_BIOZ_LC_REG reg: %08" PRIx32"\n",max_regs_val.cnfgBiozLc.all);
  INFO("write MAX30001_CNFG_RTOR1_REG reg:   %08" PRIx32"\n",max_regs_val.cnfgRtor1.all);
  INFO("write MAX30001_CNFG_RTOR2_REG reg:   %08" PRIx32"\n",max_regs_val.cnfg_rtor2.all);

  _writeReg(MAX30001_EN_INT_REG,max_regs_val.enInt.all);
  _writeReg(MAX30001_EN_INT2_REG,max_regs_val.enInt2.all);
  _writeReg(MAX30001_MNGR_INT_REG,max_regs_val.mngrInt.all);
  _writeReg(MAX30001_MNGR_DYN_REG,max_regs_val.mngrDyn.all);
  _writeReg(MAX30001_CNFG_GEN_REG,max_regs_val.cnfgGen.all);
  _writeReg(MAX30001_CNFG_CAL_REG,max_regs_val.cnfgCal.all);
  _writeReg(MAX30001_CNFG_EMUX_REG,max_regs_val.cnfgEmux.all);
  _writeReg(MAX30001_CNFG_ECG_REG,max_regs_val.cnfgEcg.all);
  _writeReg(MAX30001_CNFG_BMUX_REG,max_regs_val.cnfgBmux.all);
  _writeReg(MAX30001_CNFG_BIOZ_REG,max_regs_val.cnfgBioz.all);
  _writeReg(MAX30001_CNFG_BIOZ_LC_REG,max_regs_val.cnfgBiozLc.all);
  _writeReg(MAX30001_CNFG_RTOR1_REG,max_regs_val.cnfgRtor1.all);
  _writeReg(MAX30001_CNFG_RTOR2_REG,max_regs_val.cnfg_rtor2.all);

}

void max30001_get_ecg(void)
{

}


void max300001_get_status(void)
{
  int32_t ecg_val;
  float ecg_mv;
  max_regs_val.status.all = _readReg(0x01);
 // INFO("MAX30001_STATUS_REG reg read:     %08" PRIx32"\n",max_regs_val.status.all);
  if(max_regs_val.status.bit.eint == 1)
  {
    max_regs_val.ecg_fifo.all = _readReg(MAX30001_ECG_FIFO_REG);
    ecg_val = max_regs_val.ecg_fifo.all & 0xFFFFFF30;
    ecg_val = ecg_val << 8;
    ecg_val = ecg_val / 16384;
    ecg_val = ecg_val * 1000;
    ecg_mv = (float)ecg_val / (float)2621440;

    //ecg_mv = ((float)(max_regs_val.ecf_fifo.all>>6)*1000)/(131072*20);
    // ecg_mv = (float)(max_regs_val.ecg_fifo.all>>6);
    // ecg_mv *= 1000;
    // ecg_mv /= (131072*20);
    INFO(" %.8f\n",ecg_mv);
    //INFO("%d,%d,%d\n",(int)max_regs_val.ecg_fifo.all>>6,(int)max_regs_val.ecg_fifo.bit.etag,(int)max_regs_val.ecg_fifo.bit.ptag);
    //INFO("MAX30001_FIFO reg read:         %08" PRIx32"\n",max_regs_val.ecf_fifo.all);
    //INFO("MAX30001_ECG data, etag, ptag %d,%04 "PRIx32 "%04" PRIx32"\n",max_regs_val.ecf_fifo.bit.ecg_data,max_regs_val.ecf_fifo.bit.etag,max_regs_val.ecf_fifo.bit.ptag);
    //INFO("%08"PRIx32",%08"PRIx32"\n",max_regs_val.status.all,max_regs_val.ecg_fifo.all);
    //INFO("%d\n",(int)ecg_val);
  }
  //max30001_get_ecg();
}

static void _init(void)
{

  uint32_t regVal;
    esp_err_t ret;
    // spi_device_handle_t spi;
    spi_bus_config_t buscfg={
        .miso_io_num = SPI_PIN_NUM_MISO,
        .mosi_io_num = SPI_PIN_NUM_MOSI,
        .sclk_io_num = SPI_PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz=0
    };
    spi_device_interface_config_t devcfg={
#ifdef CONFIG_LCD_OVERCLOCK
        .clock_speed_hz=26*1000*1000,           //Clock out at 26 MHz
#else
        .clock_speed_hz=1*1000*1000,           //Clock out at 10 MHz
#endif
        .mode=0,                                //SPI mode 0
        .spics_io_num=SPI_PIN_NUM_CS,               //CS pin
        .queue_size=7,                          //We want to be able to queue 7 transactions at a time
        //.pre_cb=lcd_spi_pre_transfer_callback,  //Specify pre-transfer callback to handle D/C line
    };
    //Initialize the SPI bus
    ret=spi_bus_initialize(MAX30001_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);
    //Attach the MAX30001 to the SPI bus
    ret=spi_bus_add_device(MAX30001_HOST, &devcfg, &spi_handle);
    ESP_ERROR_CHECK(ret);

    max_regs_val.status.all = _readReg(0x01);
    INFO("MAX30001_STATUS_REG reg read:     %08" PRIx32"\n",max_regs_val.status.all);

    max30001_start_ecg();
    _writeReg(MAX30001_FIFO_RST_REG,0x00000000);
    _writeReg(MAX30001_SYNCH_REG, 0x00000000);
}

static bool dbgStatus(uint8_t argc, char** argv)
{
    PRINT("status\n");
    return true;
}


DEBUG_MENU_START(g_menu)
    DEBUG_MENU_DIR("cmd", NULL)
	    DEBUG_MENU_CMD("status",	NULL,		NULL, dbgStatus)
    DEBUG_MENU_DIR_END
DEBUG_MENU_END


void max30001_init(void)
{
    DBG_TREE_add("/", g_menu);

    _init();
}

