#pragma once

#include <sys_def.h>

#define MAX30001_WREG   0x00
#define MAX30001_RREG  0x01

#define MAX3001_REG_STATUS             0x01
#define MAX3001_REG_EN_INT             0x02
#define MAX3001_REG_EN_INT2            0x03
#define MAX3001_REG_MNGR_INT           0x04
#define MAX3001_REG_MNGR_DYN           0x05
#define MAX3001_REG_SW_RST             0x08
#define MAX3001_REG_SYNCH              0x09
#define MAX3001_REG_FIFO_RST           0x0A
#define MAX3001_REG_INFO               0x0F
#define MAX3001_REG_CNFG_GEN           0x10
#define MAX3001_REG_CNFG_CAL           0x12
#define MAX3001_REG_CNFG_EMUX          0x14
#define MAX3001_REG_CNFG_ECG           0x15
#define MAX3001_REG_CNFG_BMUX          0x17
#define MAX3001_REG_CNFG_BIOZ          0x18
#define MAX3001_REG_CNFG_BIOZ_LC       0x1A
#define MAX3001_REG_CNFG_RTOR1         0x1D
#define MAX3001_REG_CNFG_RTOR2         0x1E
#define MAX3001_REG_ECG_FIFO_BURST     0x20
#define MAX3001_REG_ECG_FIFO           0x21
#define MAX3001_REG_BIOZ_FIFO_BURST    0x22
#define MAX3001_REG_BIOZ_FIFO          0x23
#define MAX3001_REG_RTOR               0x25
#define MAX3001_REG_NO_OP              0x7F

#define CES_CMDIF_PKT_START_1 0x0A
#define CES_CMDIF_PKT_START_2 0xFA
#define CES_CMDIF_TYPE_DATA 0x02
#define CES_CMDIF_PKT_STOP 0x0B
#define DATA_LEN 0x0C
#define ZERO 0

/**
 * @brief STATUS (0x01)
 */
typedef union max30001_status_reg {
	uint32_t all;

	struct {
		uint32_t ldoff_nl : 1;
		uint32_t ldoff_nh : 1;
		uint32_t ldoff_pl : 1;
		uint32_t ldoff_ph : 1;

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
		uint32_t clr_pedge : 1;
		uint32_t clr_rrint : 2;
		uint32_t clr_fast  : 1;
		uint32_t reserved1 : 1;
		uint32_t reserved2 : 4;
		uint32_t reserved3 : 4;
		uint32_t bfit      : 3;
		uint32_t efit      : 5;

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
		uint32_t dcloff_ipol: 1;
		uint32_t en_dcloff  : 2;
		uint32_t en_bloff   : 2;
		uint32_t reserved1	: 1;
		uint32_t en_pace    : 1;
		uint32_t en_bioz    : 1;
		uint32_t en_ecg     : 1;
		uint32_t fmstr      : 2;
		uint32_t en_ulp_lon : 2;
		uint32_t reserved   : 8;
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
		uint32_t ln_bioz   : 1;
		uint32_t ext_rbias : 1;
		uint32_t ahpf      : 3;
		uint32_t rate      : 1;
		uint32_t reserved  : 8;
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
		uint32_t ptag     : 3;
		uint32_t etag     : 3;
		uint32_t data : 18;
	} bit;
} max30001_ecg_fifo_u;

typedef union max30001_bioz_fifo_reg {
	uint32_t all;
	struct {
		uint32_t btag     : 3;
		uint32_t reserved1: 1;
		uint32_t data : 20;
	} bit;
} max30001_bioz_fifo_u;

typedef struct max30001_registers {
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
} max30001_registers_t;

typedef enum {
	MAX30001_NO_INT = 0, // No interrupt
	MAX30001_INT_B  = 1,  // INTB selected for interrupt
	MAX30001_INT_2B = 2  // INT2B selected for interrupt
} max30001_intrpt_Location_t;

