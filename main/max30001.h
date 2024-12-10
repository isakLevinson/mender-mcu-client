#ifndef MAX30001_H_
#define MAX30001_H_

#include <inttypes.h>

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



#if CONFIG_BUILD_TYPE_GSR
void max30001_init(void);
void max30001_write_reg(uint8_t i_reg, uint32_t regVal);
uint32_t max30001_read_reg(uint8_t i_reg);
void max300001_get_status(void);

#else
#define max30001_init()
#define max30001_write_reg(i_reg, regVal)
#define max30001_read_reg(i_reg)          0
#define max300001_get_status()
#endif

#endif
